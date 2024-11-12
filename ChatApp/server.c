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
void handle_client(int client_sock);

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
            send(clients[i].sock, message, strlen(message), 0);
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
    send(sock, message, strlen(message), 0);
    pthread_mutex_unlock(&clients_mutex);
}

void list_chat_rooms(int sock) {
    pthread_mutex_lock(&rooms_mutex);
    char message[BUFFER_SIZE] = "Chat rooms:\n";
    for (int i = 0; i < room_count; i++) {
        strcat(message, chat_rooms[i].name);
        strcat(message, "\n");
    }
    send(sock, message, strlen(message), 0);
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

void handle_client(int client_sock) {
    char buffer[BUFFER_SIZE];
    int read_size;
    char username[50];

    if ((read_size = recv(client_sock, username, 50, 0)) > 0) {
        username[read_size] = '\0';
        pthread_mutex_lock(&clients_mutex);
        clients[client_count].sock = client_sock;
        strcpy(clients[client_count].username, username);
        client_count++;
        pthread_mutex_unlock(&clients_mutex);
        printf("%s joined the chat\n", username);
    }

    while ((read_size = recv(client_sock, buffer, BUFFER_SIZE, 0)) > 0) {
        buffer[read_size] = '\0';
        printf("Received message from %s: %s\n", username, buffer);

        buffer[strcspn(buffer, "\r\n")] = 0;

        if (strncmp(buffer, "/msg ", 5) == 0) {
            char *target_username = strtok(buffer + 5, " ");
            char *message = strtok(NULL, "\0");
            pthread_mutex_lock(&clients_mutex);
            for (int i = 0; i < client_count; i++) {
                if (strcmp(clients[i].username, target_username) == 0) {
                    send(clients[i].sock, message, strlen(message), 0);
                    break;
                }
            }
            pthread_mutex_unlock(&clients_mutex);
        } else if (strncmp(buffer, "/list", 5) == 0) {
            printf("Executing /list command\n");  
            list_online_users(client_sock);
        } else if (strncmp(buffer, "/rooms", 6) == 0) {
            printf("Executing /rooms command\n"); 
            list_chat_rooms(client_sock);
        } else if (strncmp(buffer, "/join ", 6) == 0) {
            char *room_name = buffer + 6;
            printf("Executing /join command for room: %s\n", room_name); 
            join_chat_room(room_name, &clients[client_count - 1]);
        } else if (strncmp(buffer, "/roommsg ", 9) == 0) {
            char *message = buffer + 9;
            pthread_mutex_lock(&rooms_mutex);
            for (int i = 0; i < room_count; i++) {
                if (strcmp(chat_rooms[i].name, clients[client_count - 1].room) == 0) {
                    for (int j = 0; j < chat_rooms[i].client_count; j++) {
                        if (chat_rooms[i].clients[j]->sock != client_sock) {
                            send(chat_rooms[i].clients[j]->sock, message, strlen(message), 0);
                        }
                    }
                    break;
                }
            }
            pthread_mutex_unlock(&rooms_mutex);
        } else {
            broadcast_message(buffer, client_sock);
        }
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
        handle_client(client_sock);
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