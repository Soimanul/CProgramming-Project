#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#ifdef _WIN32
#include <winsock2.h>
#pragma comment(lib, "Ws2_32.lib") 
typedef int socklen_t;  
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_CLIENTS 100
#define MAX_ROOMS 10
#define MAX_ROOM_NAME 50

typedef struct {
    int sock;
    char username[50];
    char room[MAX_ROOM_NAME];
} Client;

typedef struct {
    char name[MAX_ROOM_NAME];
    Client *clients[MAX_CLIENTS];
    int client_count;
} ChatRoom;

Client clients[MAX_CLIENTS];
ChatRoom chat_rooms[MAX_ROOMS];
int client_count = 0;
int room_count = 0;
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t rooms_mutex = PTHREAD_MUTEX_INITIALIZER;

int init_socket();
void broadcast_message(const char *message, int exclude_sock);
void list_online_users(int sock);
void list_chat_rooms(int sock);
void join_chat_room(const char *room_name, Client *client);
void create_chat_room(const char *room_name, Client *creator, char *users);
void room_message(const char *room_name, const char *message, const char *sender);
void handle_client(int client_sock);
void *client_handler(void *client_sock_ptr);

int init_socket() {
    int sock;
    struct sockaddr_in server_addr;

    #ifdef _WIN32
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("WSAStartup failed.\n");
        return -1;
    }
    #endif

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == -1) {
        perror("Socket creation failed");
        return -1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        return -1;
    }

    if (listen(sock, 3) < 0) {
        perror("Listen failed");
        return -1;
    }

    struct sockaddr_in sin;
    socklen_t len = sizeof(sin);
    if (getsockname(sock, (struct sockaddr *)&sin, &len) == -1) {
        perror("getsockname");
    } else {
        printf("Server is running on IP: %s, Port: %d\n", inet_ntoa(sin.sin_addr), ntohs(sin.sin_port));
    }

    return sock;
}

void broadcast_message(const char *message, int exclude_sock) {
    pthread_mutex_lock(&clients_mutex);
    for (int i = 0; i < client_count; i++) {
        if (clients[i].sock != exclude_sock) {
            if (send(clients[i].sock, message, strlen(message), 0) < 0) {
                perror("Send failed");
            }
        }
    }
    pthread_mutex_unlock(&clients_mutex);
}

void list_online_users(int sock) {
    pthread_mutex_lock(&clients_mutex);
    char message[BUFFER_SIZE] = "Online users:\n";
    for (int i = 0; i < client_count; i++) {
        strcat(message, clients[i].username);
        strcat(message, "\n");
    }
    if (send(sock, message, strlen(message), 0) < 0) {
        perror("Send failed");
    }
    pthread_mutex_unlock(&clients_mutex);
}

void list_chat_rooms(int sock) {
    pthread_mutex_lock(&rooms_mutex);
    char message[BUFFER_SIZE] = "Chat rooms:\n";
    for (int i = 0; i < room_count; i++) {
        strcat(message, chat_rooms[i].name);
        strcat(message, "\n");
    }
    if (send(sock, message, strlen(message), 0) < 0) {
        perror("Send failed");
    }
    pthread_mutex_unlock(&rooms_mutex);
}

void join_chat_room(const char *room_name, Client *client) {
    pthread_mutex_lock(&rooms_mutex);
    for (int i = 0; i < room_count; i++) {
        if (strcmp(chat_rooms[i].name, room_name) == 0) {
            chat_rooms[i].clients[chat_rooms[i].client_count++] = client;
            strcpy(client->room, room_name);
            printf("Client %s joined chat room %s\n", client->username, room_name); 
            pthread_mutex_unlock(&rooms_mutex);
            return;
        }
    }
    strcpy(chat_rooms[room_count].name, room_name);
    chat_rooms[room_count].clients[0] = client;
    chat_rooms[room_count].client_count = 1;
    strcpy(client->room, room_name);
    room_count++;
    printf("Chat room %s created and client %s joined\n", room_name, client->username); 
    pthread_mutex_unlock(&rooms_mutex);
}

void create_chat_room(const char *room_name, Client *creator, char *users) {
    pthread_mutex_lock(&rooms_mutex);
    strcpy(chat_rooms[room_count].name, room_name);
    chat_rooms[room_count].clients[0] = creator;
    chat_rooms[room_count].client_count = 1;
    strcpy(creator->room, room_name);

    char *username = strtok(users, " ");
    while (username != NULL) {
        for (int i = 0; i < client_count; i++) {
            if (strcmp(clients[i].username, username) == 0) {
                chat_rooms[room_count].clients[chat_rooms[room_count].client_count++] = &clients[i];
                strcpy(clients[i].room, room_name);
                break;
            }
        }
        username = strtok(NULL, " ");
    }

    room_count++;
    printf("Chat room %s created", room_name); 
    pthread_mutex_unlock(&rooms_mutex);
}

void room_message(const char *room_name, const char *message, const char *sender) {
    pthread_mutex_lock(&rooms_mutex);
    char full_message[BUFFER_SIZE];
    snprintf(full_message, BUFFER_SIZE, "%s/%s: %s", room_name, sender, message);

    for (int i = 0; i < room_count; i++) {
        if (strcmp(chat_rooms[i].name, room_name) == 0) {
            for (int j = 0; j < chat_rooms[i].client_count; j++) {
                if (strcmp(chat_rooms[i].clients[j]->username, sender) != 0) {
                    if (send(chat_rooms[i].clients[j]->sock, full_message, strlen(full_message), 0) < 0) {
                        perror("Send failed");
                    }
                }
            }
            break;
        }
    }
    pthread_mutex_unlock(&rooms_mutex);
}

void display_menu(int client_sock) {
    const char *menu = 
        "Menu:\n"
        "1. Private message\n"
        "2. Create a room\n"
        "3. Send a room message\n"
        "4. List online users\n"
        "5. List chat rooms\n"
        "6. Join a chat room\n"
        "7. Broadcast a message\n"
        "Enter your choice: ";
    send(client_sock, menu, strlen(menu), 0);
}

void handle_client(int client_sock) {
    char buffer[BUFFER_SIZE];
    int read_size;
    char username[50];

    if ((read_size = recv(client_sock, username, 50, 0)) > 0) {
        username[read_size] = '\0';
        username[strcspn(username, "\r\n")] = 0;
        pthread_mutex_lock(&clients_mutex);
        clients[client_count].sock = client_sock;
        strcpy(clients[client_count].username, username);
        client_count++;
        pthread_mutex_unlock(&clients_mutex);
        printf("%s joined the chat\n", username);
    }

    display_menu(client_sock);

    while ((read_size = recv(client_sock, buffer, BUFFER_SIZE, 0)) > 0) {
        buffer[read_size] = '\0';
        printf("Received message from %s: %s\n", username, buffer);

        buffer[strcspn(buffer, "\r\n")] = 0;

        int choice = atoi(buffer);
        switch (choice) {
            case 1: {
                send(client_sock, "Enter target username: ", 23, 0);
                read_size = recv(client_sock, buffer, BUFFER_SIZE, 0);
                buffer[read_size] = '\0';
                char target_username[50];
                strcpy(target_username, buffer);
                target_username[strcspn(target_username, "\r\n")] = 0;

                send(client_sock, "Enter message: ", 15, 0);
                read_size = recv(client_sock, buffer, BUFFER_SIZE, 0);
                buffer[read_size] = '\0';
                char message[BUFFER_SIZE];
                strcpy(message, buffer);
                message[strcspn(message, "\r\n")] = 0;

                int found = 0;
                pthread_mutex_lock(&clients_mutex);
                for (int i = 0; i < client_count; i++) {
                    printf("Checking client: %s\n", clients[i].username);
                    if (strcmp(clients[i].username, target_username) == 0) {
                        printf("Client found: %s\n", target_username);
                        char full_message[BUFFER_SIZE];
                        snprintf(full_message, BUFFER_SIZE, "%s: %s", username, message);
                        if (send(clients[i].sock, full_message, strlen(full_message), 0) < 0) {
                            perror("Send failed");
                        } else {
                            printf("Message sent to %s\n", target_username);
                        }
                        found = 1;
                        break;
                    }
                }
                pthread_mutex_unlock(&clients_mutex);

                if (!found) {
                    if (send(client_sock, "User not found.\n", 17, 0) < 0) {
                        perror("Send failed");
                    }
                    printf("Client not registered: %s\n", target_username);
                }
                break;
            }
            case 2: {
                send(client_sock, "Enter room name: ", 17, 0);
                read_size = recv(client_sock, buffer, BUFFER_SIZE, 0);
                buffer[read_size] = '\0';
                char room_name[MAX_ROOM_NAME];
                strcpy(room_name, buffer);
                room_name[strcspn(room_name, "\r\n")] = 0;

                send(client_sock, "Enter usernames (space-separated): ", 35, 0);
                read_size = recv(client_sock, buffer, BUFFER_SIZE, 0);
                buffer[read_size] = '\0';
                char users[BUFFER_SIZE];
                strcpy(users, buffer);
                users[strcspn(users, "\r\n")] = 0;

                create_chat_room(room_name, &clients[client_count - 1], users);
                break;
            }
            case 3: {
                send(client_sock, "Enter room name: ", 17, 0);
                read_size = recv(client_sock, buffer, BUFFER_SIZE, 0);
                buffer[read_size] = '\0';
                char room_name[MAX_ROOM_NAME];
                strcpy(room_name, buffer);
                room_name[strcspn(room_name, "\r\n")] = 0;

                send(client_sock, "Enter message: ", 15, 0);
                read_size = recv(client_sock, buffer, BUFFER_SIZE, 0);
                buffer[read_size] = '\0';
                char message[BUFFER_SIZE];
                strcpy(message, buffer);
                message[strcspn(message, "\r\n")] = 0;

                room_message(room_name, message, username);
                break;
            }
            case 4:
                list_online_users(client_sock);
                break;
            case 5:
                list_chat_rooms(client_sock);
                break;
            case 6: {
                send(client_sock, "Enter room name: ", 17, 0);
                read_size = recv(client_sock, buffer, BUFFER_SIZE, 0);
                buffer[read_size] = '\0';
                char room_name[MAX_ROOM_NAME];
                strcpy(room_name, buffer);
                room_name[strcspn(room_name, "\r\n")] = 0;

                join_chat_room(room_name, &clients[client_count - 1]);
                break;
            }
            case 7: {
                send(client_sock, "Enter message: ", 15, 0);
                read_size = recv(client_sock, buffer, BUFFER_SIZE, 0);
                buffer[read_size] = '\0';
                char message[BUFFER_SIZE];
                strcpy(message, buffer);
                message[strcspn(message, "\r\n")] = 0;

                broadcast_message(message, client_sock);
                break;
            }
            default:
                send(client_sock, "Invalid choice. Please try again.\n", 34, 0);
                break;
        }

        display_menu(client_sock);
    }

    if (read_size == 0) {
        printf("Client disconnected\n");
    } else if (read_size == -1) {
        perror("Recv failed");
    }

    #ifdef _WIN32
    closesocket(client_sock);
    #else
    close(client_sock);
    #endif
}

void *client_handler(void *client_sock_ptr) {
    int client_sock = *(int *)client_sock_ptr;
    free(client_sock_ptr); 
    handle_client(client_sock);
    return NULL;
}

int main() {
    int server_sock, client_sock;
    struct sockaddr_in client_addr;
    socklen_t client_addr_len = sizeof(client_addr);

    server_sock = init_socket();
    if (server_sock == -1) {
        return 1;
    }

    printf("Server listening on port %d\n", PORT);

    while ((client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &client_addr_len)) >= 0) {
        printf("Client connected\n");
        pthread_t tid;
        int *client_sock_ptr = malloc(sizeof(int)); 
        *client_sock_ptr = client_sock;

        if (pthread_create(&tid, NULL, client_handler, (void *)client_sock_ptr) != 0) {
            perror("Thread creation failed");
        }
    }

    if (client_sock < 0) {
        perror("Accept failed");
    }

    #ifdef _WIN32
    closesocket(server_sock);
    WSACleanup();
    #else
    close(server_sock);
    #endif

    return 0;
}