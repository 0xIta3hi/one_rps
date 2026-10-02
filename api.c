// create a basic TCP server right now
#include<stdio.h>
#include<stdlib.h>
#include<string.h>
#include<unistd.h>
#include<arpa/inet.h>
#include<sys/socket.h>

#define PORT 3000
#define BUFFER_SIZE 1024

int main(){
    int server_fd, client_fd;
    struct sockaddr_in server_addr,client_addr;
    socklen_t addr_len = sizeof(client_addr);
    char buf[BUFFER_SIZE];

    //creating a socket file desciptor (IPv4, TCP, default protocol)
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if(server_fd < 0) {
        perror("failed to create socket");  
        exit(EXIT_FAILURE);
    }

    // server address structure
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    // bind the socket to ip address and port
    if(bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0){
        perror("Bind failure");
        close(server_fd);
        exit(EXIT_FAILURE);
    }
    // start listening
    if(listen(server_fd, 5) < 0){
        perror("Listening failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("TCP server running on port %d.. \n", PORT);

    while(1){
        //continuously accept new client connections.
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
        if(client_fd < 0){
            perror("Client side failure");
            continue;
        }
    }
    printf("Client connected from IP %s, Port %d", inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port));

        while (1){
            memset(buf, 0, BUFFER_SIZE);
            
            // Read data from client
            ssize_t bytes_read = read(client_fd, buf, BUFFER_SIZE - 1);
            if (bytes_read <= 0) {
                // If read returns 0, client disconnected gracefully. If < 0, an error occurred.
                if (bytes_read == 0) {
                    printf("Client disconnected.\n");
                } else {
                    perror("Read error");
                }
                break;
            }

            printf("Received: %s", buf);

            // Echo the message back to the client
            write(client_fd, buf, bytes_read);

        // 8. Close the specific client socket and wait for a new connection
            close(client_fd);
        }
        close(server_fd);
        return 0;
    }
