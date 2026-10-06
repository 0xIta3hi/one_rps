#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include<pthread.h>

#define PORT 8080
#define BUFFER_SIZE 1024

void* handle_client(void* arg){

    printf("handling client.");
    int client_fd = *(int* )arg; 
    free(arg);

    char buffer[BUFFER_SIZE];
     while (1) {
            memset(buffer, 0, BUFFER_SIZE);           
            // Read data from client
            ssize_t bytes_read = read(client_fd, buffer, BUFFER_SIZE - 1);
            if (bytes_read <= 0) {
                // If read returns 0, client disconnected gracefully. If < 0, an error occurred.
                if (bytes_read == 0) {
                    printf("Client disconnected.\n");
                } else {
                    perror("Read error");
                }
                break;
            }

            printf("Received: %s", buffer);

            // Echo the message back to the client
            write(client_fd, buffer, bytes_read);
        }

        // 8. Close the specific client socket and wait for a new connection
        close(client_fd);
        return NULL;
}

int main() {
    int server_fd, client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_len = sizeof(client_addr);
    int opt = 1;

    // 1. Create socket file descriptor (IPv4, TCP, default protocol)
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // 2. Set socket options to avoid "Address already in use" errors on restart
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // 3. Define the server address structure
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; // Accept connections on any network interface
    server_addr.sin_port = htons(PORT);       // Convert port to network byte order (Endianness)

    // 4. Bind the socket to the port and IP address
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // 5. Start listening for incoming connections (Queue up to 5 pending connections)
    if (listen(server_fd, 5) < 0) {
        perror("Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("TCP Server running on port %d...\n", PORT);

    // Infinite loop to continuously handle incoming client connections
    while (1) {
        addr_len = sizeof(client_addr);
        // 6. Accept a client connection (Blocks execution until a client connects)
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
        if (client_fd < 0) {
            perror("Accept failed");
            continue; // Skip to next iteration rather than crashing the server
        }
        int *client = malloc(sizeof(int));
        if(client == NULL){
            perror("malloc failed");
            close(client_fd);
            continue;
        }
        *client = client_fd;
        // implement multi-threading here,
        // if accepted -> create a new thread -> handle client requests
        // create the working thread.
        pthread_t thread;
        if(pthread_create(&thread, NULL, handle_client, client)!=0){
            perror("thread creation failed");
            free(client);
            close(client_fd);
            continue;
        }

        pthread_detach(thread);
        printf("Client connected from IP: %s, Port: %d\n", 
               inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));
        }
        // 7. Data exchange loop with the connected client
    close(server_fd);
    return 0;
}
