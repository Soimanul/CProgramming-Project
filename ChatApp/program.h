#ifndef PROGRAM_H
#define PROGRAM_H

#include <stdio.h>      // For input/output functions like printf, fgets, and perror
#include <stdlib.h>     // For dynamic memory allocation and general utilities
#include <string.h>     // For string manipulation functions llike strcmp, strlen, strcspn
#include <pthread.h>    // For thread handling with pthread_create

#ifdef _WIN32
// Windows-specific libraries
#include <winsock2.h>   // For socket programming on Windows ONLY
#include <ws2tcpip.h>   // For InetPton and related network functions
#pragma comment(lib, "Ws2_32.lib")  // Link with Ws2_32.lib for Windows socket functionality
typedef int socklen_t;  // Define socklen_t for Windows compatibility
#else
// Non-Windows-specific libraries
#include <sys/socket.h> // For socket creation and connection
#include <netinet/in.h> // For sockaddr_in and related network structures
#include <arpa/inet.h>  // For inet_pton to convert IP addresses
#include <unistd.h>     // For close function to close sockets
#endif

#define PORT 8080           // Port number for server
#define BUFFER_SIZE 1024    // Size of the bufffer for mesages
#define MAX_CLIENTS 100     // Maximum number of clients server handles
#define MAX_ROOMS 10        // Maximum number of chat rooms
#define MAX_ROOM_NAME 50    // Maximum length of a room name


// global structures
typedef struct {
    int sock;
    char username[50];
    char room[MAX_ROOM_NAME];
} Client;

typedef struct {
    char name[MAX_ROOM_NAME];
    Client *clients[MAX_CLIENTS];
    int client_count;
    char creator_username[50]; 
} ChatRoom;


void *receive_messages(void *sock); 

// Global Variables (extern declaration)
extern Client clients[MAX_CLIENTS];           // Array clients
extern ChatRoom chat_rooms[MAX_ROOMS];        // Array chat rooms
extern int client_count;                      // Number of connected clients
extern int room_count;                        // Number chat rooms
extern pthread_mutex_t clients_mutex;         // Mutex used for clients array
extern pthread_mutex_t rooms_mutex;           // Mutex used for chat rooms array

// Function Prototypes
int init_socket();  // Initialize server socket and return the socket descriptor

void private_message(int client_sock, const char *username);
void create_chat_room(int client_sock);  // Create a new chat room
void message_chatroom(int client_sock, const char *sender); // Send a message to all clients in a room
void list_online_users(int sock);  // List all online users for a client
void list_chat_rooms(int sock);    // List all available chat rooms for a client
void join_chat_room(int client_sock, Client *client);  // Join a chat room
void broadcast_message(int client_sock); // Broadcast message to clients in the same room (excluding a specific socket which is the sender)

void handle_client(int client_sock);  // Handles menu cases
void *client_handler(void *client_sock_ptr);  // Threaded function to handle client communic. 

#endif