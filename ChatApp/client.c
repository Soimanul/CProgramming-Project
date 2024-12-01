#include "program.h"

const char *LAN = "192.168.56.1"; // replace with YOUR OWN IP
const char *LOCAL = "127.0.0.1";

// runs in its own thread
void *receive_messages(void *sock) {
    int server_sock = *(int *)sock; // sock is casted to an int pointer, then we get the socket descriptor from it
    char buffer[BUFFER_SIZE]; // will hold incoming data for a small amount of time
    int read_size; // stores # of bytes succesfully read during the recv call

    // the client should be ready to recieve messages as soon as it connects to the server
    while ((read_size = recv(server_sock, buffer, BUFFER_SIZE, 0)) > 0) {
        buffer[read_size] = '\0';
        printf("%s\n", buffer);
        if (strcmp(buffer, "Goodbye!") == 0) {
            printf("Client successfully disconnected.\n");
            exit(0);  //  exit the loop when the servr disconnects
        }
    }

    if (read_size == 0) {
        printf("Server disconnected\n");
    } else if (read_size == -1) {
        perror("Receive failed");
    }

    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <LAN|LOCAL>\n", argv[0]);
        return 1;}

    const char *server_ip;
    if (strcmp(argv[1], "LAN") == 0) {
        server_ip = LAN;
    } else if (strcmp(argv[1], "LOCAL") == 0) {
        server_ip = LOCAL;
    } else {
        fprintf(stderr, "Invalid argument. Please use 'LAN' or 'LOCAL'.\n");
        return 1;
    }

    #ifdef _WIN32
    WSADATA wsaData;
    SOCKET sock;
    struct sockaddr_in server_addr;
    char message[BUFFER_SIZE];

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("WSAStartup failed\n");
        exit(1);
        }

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        printf("Socket creation failed\n");
        WSACleanup();
        exit(1);
        }
    #else
    int sock;
    struct sockaddr_in server_addr;
    char message[BUFFER_SIZE];

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket creation failed");
        return 1;
    }
    #endif

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);

    #ifdef _WIN32
    if (InetPton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        printf("Invalid address/ Address not supported\n");
        return 1;
    }
    #else
    if (inet_pton(AF_INET, server_ip, &server_addr.sin_addr) <= 0) {
        printf("Invalid address/ Address not supported\n");
        return 1;
    }
    #endif

    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Connection failed");
        return 1;
    }

    printf("Connected to server. Enter your username: ");
    fgets(message, BUFFER_SIZE, stdin);
    message[strcspn(message, "\n")] = 0;  
    send(sock, message, strlen(message), 0);

    pthread_t recv_thread;
    if (pthread_create(&recv_thread, NULL, receive_messages, (void*)&sock) != 0) {
        perror("Could not create thread");
        return 1;
    }

    while (fgets(message, BUFFER_SIZE, stdin) != NULL) {
        send(sock, message, strlen(message), 0);

        // Exit condition
        if (strcmp(message, "exit\n") == 0) {
            printf("Exiting...\n");
            break; // Exit the loop
        }
    }

    // Make sure that the receive thread is finished before cleaning everything up
    pthread_join(recv_thread, NULL);
    #ifdef _WIN32
    closesocket(sock);
    WSACleanup();
    #else
    close(sock);
    #endif

    return 0;
}