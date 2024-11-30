#include "program.h"


// Global Variables
Client clients[MAX_CLIENTS];           // Initialize the array of clients
ChatRoom chat_rooms[MAX_ROOMS];        // Initialize the array of chat rooms
int client_count = 0;                  // Initialize the client count to 0
int room_count = 0;                    // Initialize the room count to 0
pthread_mutex_t clients_mutex = PTHREAD_MUTEX_INITIALIZER;  // Initialize the mutex for clients
pthread_mutex_t rooms_mutex = PTHREAD_MUTEX_INITIALIZER;    // Initialize the mutex for rooms


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

void private_message(int client_sock, const char *username) { // uses client socket for dirct communication, and usernmae 
    char buffer[BUFFER_SIZE]; // temp storage

    char target_username[50];
    char message[BUFFER_SIZE];
    int read_size;
    
    // prompt target user through personal socket 
    send(client_sock, "Enter target username: ", 23, 0);

    // server recieves answer
    read_size = recv(client_sock, buffer, BUFFER_SIZE, 0);
    buffer[read_size] = '\0'; // adds null to be able to transform into a string with no issues
    strcpy(target_username, buffer); 
    target_username[strcspn(target_username, "\r\n")] = 0;  // handles possible \n  to clean input

    int found = 0;
    pthread_mutex_lock(&clients_mutex);  //This is what we use to avoid collisions, it avoids other threads to access same resource 
    for (int i = 0; i < client_count; i++) { 
        if (strcmp(clients[i].username, target_username) == 0) { // loop continues until we find target usernmae
            found = 1;
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);  //we must stop blocking the clients array

    // other possible error: not finidng the username
    if (!found) {
        if (send(client_sock, "User was not found.\n", 21, 0) < 0) { // sends client
            perror("Send failed");
        }
        printf("Client not registered: %s\n", target_username); // prints in server's console
        return;
    }

    // asks user for desired mesage to send
    send(client_sock, "Enter message: ", 15, 0);

    //Server gets the message
    read_size = recv(client_sock, buffer, BUFFER_SIZE, 0);
    buffer[read_size] = '\0';
    strcpy(message, buffer);
    message[strcspn(message, "\r\n")] = 0;  

    pthread_mutex_lock(&clients_mutex);  //This is what we use to avoid collisions, it avoids other threads to access same resource 
    for (int i = 0; i < client_count; i++) { 
        if (strcmp(clients[i].username, target_username) == 0) { // loop continues until we find target usernmae
            char full_message[BUFFER_SIZE];
            snprintf(full_message, BUFFER_SIZE, "%s: %s", username, message);  // usernme: message format
            if (send(clients[i].sock, full_message, strlen(full_message), 0) < 0) { //we need to send error message to client if this process fails 
                perror("Send failed");
            } else {
                printf("Message sent to %s\n", target_username); //else me we let client know message was sent
            }
            break;
        }
    }
    pthread_mutex_unlock(&clients_mutex);  //we must stop blocking the clients array
}


void create_chat_room(int client_sock) { // uses client's socket descriptor 
    char buffer[BUFFER_SIZE];
    int read_size;
    char room_name[MAX_ROOM_NAME];
    char users[BUFFER_SIZE];

    // Prompts for desired room name
    send(client_sock, "Enter room name: ", 17, 0);
    read_size = recv(client_sock, buffer, BUFFER_SIZE, 0);
    buffer[read_size] = '\0';
    strcpy(room_name, buffer);
    room_name[strcspn(room_name, "\r\n")] = 0;

    pthread_mutex_lock(&rooms_mutex);

    // Check if there's already a room with that name
    for (int i = 0; i < room_count; i++) {
        if (strcmp(chat_rooms[i].name, room_name) == 0) {
            printf("Error: Chat room '%s' already exists.\n", room_name); // Error message if room already exists
            send(client_sock, "Error: Chat room already exists.\n", 34, 0);
            pthread_mutex_unlock(&rooms_mutex);
            return;
        }
    }

    pthread_mutex_unlock(&rooms_mutex);

    // Asks for usernames to add to the room
    send(client_sock, "Enter usernames (space-separated): ", 35, 0);

    read_size = recv(client_sock, buffer, BUFFER_SIZE, 0);
    buffer[read_size] = '\0'; 
    strcpy(users, buffer); 
    users[strcspn(users, "\r\n")] = 0; 

    // Find creator's socket to place in the first position of the array for consistency
    Client *room_creator = NULL; // pointer to room_creator
    pthread_mutex_lock(&clients_mutex); 
    for (int i = 0; i < client_count; i++) {
        if (clients[i].sock == client_sock) {
            room_creator = &clients[i]; // holds reference to client struct 
            break;
        }
    }

    if (room_creator) {
        pthread_mutex_lock(&rooms_mutex); // Lock rooms mutex before creating room

        // Create the room and assign the creator
        strcpy(chat_rooms[room_count].name, room_name);
        strcpy(chat_rooms[room_count].creator_username, room_creator->username);  // Store the creator's username
        chat_rooms[room_count].clients[0] = room_creator; // Now it is in idx 0 of chat_rooms array 
        chat_rooms[room_count].client_count = 1; // Update chatroom's client count 
        strcpy(room_creator->room, room_name); // Updates the chatroom array in client struct 

        printf("Creator room set to: %s\n", room_creator->room);

        // Add users to the room
        char *username = strtok(users, " "); // Tokenizes to extract sub arrays 
        while (username != NULL) {
            int users_found = 0;
            for (int i = 0; i < client_count; i++) { // looks for users
                if (strcmp(clients[i].username, username) == 0) {
                    printf("Adding user to room: %s\n", username);
                    chat_rooms[room_count].clients[chat_rooms[room_count].client_count++] = &clients[i];
                    strcpy(clients[i].room, room_name);
                    printf("User %s room set to: %s\n", username, clients[i].room);
                    users_found = 1;
                    break;
                }
            }
            if (!users_found) { // If the user doesn't exist
                char error_message[BUFFER_SIZE];
                snprintf(error_message, BUFFER_SIZE, "Error: User '%s' does not exist.\n", username);
                send(client_sock, error_message, strlen(error_message), 0);
                printf("Error: User '%s' does not exist.\n", username);
            }
            username = strtok(NULL, " "); // Move to the next subarray
        }

        room_count++;
        printf("Room created. Room count: %d, Room clients: %d\n", room_count, chat_rooms[room_count - 1].client_count);
        pthread_mutex_unlock(&rooms_mutex); // Unlock rooms_mutex after creating the room

        // Sends confirmation to the client who created the room
        send(client_sock, "Room created successfully!\n", 26, 0);
    } else {
        pthread_mutex_unlock(&clients_mutex); // Unlock in case room_creator is not found
        // Possible edge case: couldn't find room creator
        send(client_sock, "Error creating room\n", 20, 0);
    }

    pthread_mutex_unlock(&clients_mutex); // Ensure mutex is unlocked
}

void message_chatroom(int client_sock, const char *sender) {
    char buffer[BUFFER_SIZE];
    int read_size;
    char room_name[MAX_ROOM_NAME];
    char message[BUFFER_SIZE];

    // aSk for chatroom's name user wants to message 
    send(client_sock, "Enter room name: ", 17, 0);
    // server recieves message
    read_size = recv(client_sock, buffer, BUFFER_SIZE, 0);
    buffer[read_size] = '\0';
    strcpy(room_name, buffer);
    room_name[strcspn(room_name, "\r\n")] = 0;

    // making sure the room exists or not
    int is_room_found = 0;
    int is_sender_member = 0;
    pthread_mutex_lock(&rooms_mutex); // locks chatrooms array
    for (int i = 0; i < room_count; i++) {
        if (strcmp(chat_rooms[i].name, room_name) == 0) {
            is_room_found = 1;

            // we should check that the sender is actually part of the room
            for (int j = 0; j < chat_rooms[i].client_count; j++) {
                if (strcmp(chat_rooms[i].clients[j]->username, sender) == 0) {
                    is_sender_member = 1;
                    break;
                }
            }
            break;
        }
    }
    // If room doesn't exist, notify client and return without allowing message input
    if (!is_room_found) {
        printf("Error: Chat room '%s' does not exist.\n", room_name);
        send(client_sock, "Error: Chat room does not exist.\n", 34, 0);
        pthread_mutex_unlock(&rooms_mutex);
        return; // exit the function so they can't enter a message
    }
    if (!is_sender_member) {
        send(client_sock, "Error: You are not a member of this room.\n", 42, 0);
        pthread_mutex_unlock(&rooms_mutex);
        return; // exit the function so they can't enter a message
    }
    // prompt for message user wants to deliver 
    send(client_sock, "Enter message: ", 15, 0);

    // server recieves message
    read_size = recv(client_sock, buffer, BUFFER_SIZE, 0);
    buffer[read_size] = '\0';
    strcpy(message, buffer);
    message[strcspn(message, "\r\n")] = 0;

    char full_message[BUFFER_SIZE];
    snprintf(full_message, BUFFER_SIZE, "%s/%s: %s", room_name, sender, message); // format print statement "chatroom name/sender: Message"

    // Find room in chat_rooms to send message 
    for (int i = 0; i < room_count; i++) {
        if (strcmp(chat_rooms[i].name, room_name) == 0) {
            // once chat room is found, we can now send the message (to all except sender)
            for (int j = 0; j < chat_rooms[i].client_count; j++) {
                if (strcmp(chat_rooms[i].clients[j]->room, room_name) == 0 &&
                    strcmp(chat_rooms[i].clients[j]->username, sender) != 0) {
                    printf("Sending message to: %s\n", chat_rooms[i].clients[j]->username);

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

void list_online_users(int sock) {
    pthread_mutex_lock(&clients_mutex); // locks clients array 

    char message[BUFFER_SIZE] = "Online users:\n";
    for (int i = 0; i < client_count; i++) {
        strcat(message, clients[i].username); // appends username to message
        strcat(message, "\n");
    }
    if (send(sock, message, strlen(message), 0) < 0) {  
        perror("Send failed");
    }
    pthread_mutex_unlock(&clients_mutex);
}

void list_chat_rooms(int sock) {
    pthread_mutex_lock(&rooms_mutex); // locks chat room array
    if (room_count == 0) {
        // If there are no chat rooms, send a message indicating so
        const char *no_rooms_message = "No chat rooms available.\n";
        if (send(sock, no_rooms_message, strlen(no_rooms_message), 0) < 0) {
            perror("Send failed");
        }
    } else {
        char message[BUFFER_SIZE] = "Chat rooms:\n";
        for (int i = 0; i < room_count; i++) {
            strcat(message, chat_rooms[i].name);
            strcat(message, "\n");
        }
        if (send(sock, message, strlen(message), 0) < 0) {
            perror("Send failed");
        }
    }
    pthread_mutex_unlock(&rooms_mutex);
}


void join_chat_room(int client_sock, Client *client) {
    pthread_mutex_lock(&rooms_mutex); // locks chat rooms array 
    
    char buffer[BUFFER_SIZE];
    int read_size;
    char room_name[MAX_ROOM_NAME];

    //Requests for chat room name 
    send(client_sock, "Enter room name: ", 17, 0);

    // it recieves answr 
    read_size = recv(client_sock, buffer, BUFFER_SIZE, 0);
    buffer[read_size] = '\0';
    strcpy(room_name, buffer);
    room_name[strcspn(room_name, "\r\n")] = 0;

    // we make sure the user isn't already in the room
    if (strcmp(client->room, room_name) == 0) {
        printf("Error: User '%s' is already in the room '%s'.\n", client->username, room_name);
        send(client_sock, "Error: You are already in this room.\n", 37, 0);
        pthread_mutex_unlock(&rooms_mutex);
        return;
    }

    // loop thorugh existing chat rooms 
    for (int i = 0; i < room_count; i++) {
        if (strcmp(chat_rooms[i].name, room_name) == 0) {
            chat_rooms[i].clients[chat_rooms[i].client_count++] = client; // adds client and increases counter
            strcpy(client->room, room_name); // updated client's struct 
            printf("Client %s joined chat room %s\n", client->username, room_name);
            pthread_mutex_unlock(&rooms_mutex);
            return;
        }
    }

    // checking for the edge case of a room that doesnt exist
    printf("Error: Chat room '%s' does not exist.\n", room_name);
    send(client_sock, "Error: Chat room does not exist.\n", 34, 0);

    pthread_mutex_unlock(&rooms_mutex);
}

void broadcast_message(int client_sock) {
    pthread_mutex_lock(&clients_mutex);
    
    char buffer[BUFFER_SIZE];
    int read_size;
    char message[BUFFER_SIZE];

    // Receive message from the client
    send(client_sock, "Enter message: ", 15, 0);
    read_size = recv(client_sock, buffer, BUFFER_SIZE, 0);
    buffer[read_size] = '\0';
    strcpy(message, buffer);
    message[strcspn(message, "\r\n")] = 0;

    // Broadcast the message to all clients excluding the sender
    for (int i = 0; i < client_count; i++) {
        if (clients[i].sock != client_sock) {
            if (send(clients[i].sock, message, strlen(message), 0) < 0) {
                perror("Send failed");
            }
        }
    }

    pthread_mutex_unlock(&clients_mutex);
}




void display_menu(int client_sock) {
    const char *menu =
        "----------------------\n"
        "     CHAT ACTIONS     \n"
        "----------------------\n"
        "1. Private message\n"
        "2. Create a room\n"
        "3. Send a room message\n"
        "4. List online users\n"
        "5. List chat rooms\n"
        "6. Join a chat room\n"
        "7. Broadcast a message\n"
        "8. Exit program\n"
        "Enter your choice: ";
    send(client_sock, menu, strlen(menu), 0);
}

void disconnect_client(int client_sock, const char *username) {
    pthread_mutex_lock(&clients_mutex);

    // Find the client in the clients array
    for (int i = 0; i < client_count; i++) {
        if (clients[i].sock == client_sock) {
            // Shift remaining clients
            for (int j = i; j < client_count - 1; j++) {
                clients[j] = clients[j + 1];
            }
            client_count--;
            break;
        }
    }

    pthread_mutex_unlock(&clients_mutex);
    printf("Client '%s' disconnected\n", username);

    // Inform the client
    send(client_sock, "Goodbye!", 8, 0);

    #ifdef _WIN32
    closesocket(client_sock);
    #else
    close(client_sock);
    #endif
}


void handle_client(int client_sock) {
    char buffer[BUFFER_SIZE];
    int read_size;
    char username[50];
    int unique;

    do {
        unique = 1; // Assume the username is unique initially

        // Receive the username from the client
        if ((read_size = recv(client_sock, username, 50, 0)) > 0) {
            username[read_size] = '\0';
            username[strcspn(username, "\r\n")] = 0; // Clean up input

            pthread_mutex_lock(&clients_mutex); // Lock to check existing usernames
            for (int i = 0; i < client_count; i++) {
                if (strcmp(clients[i].username, username) == 0) {
                    unique = 0; // Username already exists
                    break;
                }
            }
            pthread_mutex_unlock(&clients_mutex); // Unlock after checking

            if (!unique) {
                send(client_sock, "Username already taken. Try again.\nEnter your username:", 56, 0);
            }
        } 
    } while (!unique);
    pthread_mutex_lock(&clients_mutex);
    clients[client_count].sock = client_sock;
    strcpy(clients[client_count].username, username);
    client_count++;
    pthread_mutex_unlock(&clients_mutex);
    printf("%s joined the chat\n", username);

    display_menu(client_sock);


    while ((read_size = recv(client_sock, buffer, BUFFER_SIZE, 0)) > 0) {
        buffer[read_size] = '\0';
        printf("Received message from %s: %s\n", username, buffer);
        buffer[strcspn(buffer, "\r\n")] = 0;

        int choice = atoi(buffer);
        switch (choice) {
            case 1: {
                private_message(client_sock, username);
                break;  
            }
            case 2: {
                create_chat_room(client_sock);
                break; 
            }
            
            case 3: {
                message_chatroom(client_sock, username);
                break;
            }
            case 4:
                list_online_users(client_sock);
                break;
            case 5:
                list_chat_rooms(client_sock);
                break;
            case 6: {
                join_chat_room(client_sock, &clients[client_count - 1]);
                break;
            }
            case 7: {
                broadcast_message(client_sock);
                break;
            }
            case 8:
                disconnect_client(client_sock, username);
                return;
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
        perror("Accept failed");}


    #ifdef _WIN32
    closesocket(server_sock);
    WSACleanup();
    #else
    close(server_sock);
    #endif


    return 0;
}