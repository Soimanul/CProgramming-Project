#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>

#ifdef _WIN32
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "Ws2_32.lib") 
#else
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#endif

#define PORT 8080
#define BUFFER_SIZE 1024

const char *LAN = "10.93.122.232";
const char *LOCAL = "127.0.0.1";

void *receive_messages(void *sock) {
    int server_sock = *(int *)sock;
    char buffer[BUFFER_SIZE];
    int read_size;

    while ((read_size = recv(server_sock, buffer, BUFFER_SIZE, 0)) > 0) {
        buffer[read_size] = '\0';
        printf("%s\n", buffer);
    }

    if (read_size == 0) {
        printf("Server disconnected\n");
    } else if (read_size == -1) {
        perror("Recv failed");
    }

    return NULL;
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <LAN|LOCAL>\n", argv[0]);
        return 1;
    }

    const char *server_ip;
    if (strcmp(argv[1], "LAN") == 0) {
        server_ip = LAN;
    } else if (strcmp(argv[1], "LOCAL") == 0) {
        server_ip = LOCAL;
    } else {
        fprintf(stderr, "Invalid argument. Use 'LAN' or 'LOCAL'.\n");
        return 1;
    }

    #ifdef _WIN32
    WSADATA wsaData;
    SOCKET sock;
    struct sockaddr_in server_addr;
    char message[BUFFER_SIZE];

    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("WSAStartup failed with error code %d\n", WSAGetLastError());
        return 1;
    }

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        printf("Socket creation failed with error code %d\n", WSAGetLastError());
        WSACleanup();
        return 1;
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

    printf("Enter messages to send:\n");

    while (fgets(message, BUFFER_SIZE, stdin) != NULL) {
        send(sock, message, strlen(message), 0);
    }

    #ifdef _WIN32
    closesocket(sock);
    WSACleanup();
    #else
    close(sock);
    #endif

    return 0;
}