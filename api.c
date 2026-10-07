#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <pthread.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <errno.h>

#define PORT 8080
#define BUFFER_SIZE 1024
#define MAX_EVENTS 64

int set_non_blocking(int fd){
    int flags = fcntl(fd, F_GETFL, 0);
    return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

int handle_client(int client_fd){
    printf("handling client.\n");

    char buffer[BUFFER_SIZE];
    memset(buffer, 0, BUFFER_SIZE);           
    ssize_t bytes_read = read(client_fd, buffer, BUFFER_SIZE - 1);
    
    if(bytes_read < 0){
        if(errno == EAGAIN || errno == EWOULDBLOCK){
            return 0; // No data left to read right now
        }
        perror("Read Error");
        close(client_fd);
        return -1;
    }
    if(bytes_read == 0){
        printf("Client disconnected.\n");
        close(client_fd);
        return -1;
    }

    printf("Received: %s", buffer);

    // Echo the message back to the client
    write(client_fd, buffer, bytes_read);

    return 0;
}

void run_server_loop(int server_fd){
    int epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) {
        perror("epoll_create1 failed");
        return;
    }
    
    struct epoll_event ev, events[MAX_EVENTS];

    ev.events = EPOLLIN;
    ev.data.fd = server_fd;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev);

    while(1){
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (nfds < 0) {
            if (errno == EINTR) continue;
            perror("epoll_wait failed");
            break;
        }

        for(int i = 0; i < nfds; ++i){
            if(events[i].data.fd == server_fd){
                // New connection arriving on server socket
                int client_fd = accept(server_fd, NULL, NULL);
                if(client_fd >= 0){
                    set_non_blocking(client_fd);
                    ev.events = EPOLLIN;
                    ev.data.fd = client_fd;
                    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev);
                    printf("New client connected on fd %d\n", client_fd);
                }
            } else {
                // Existing client socket has data
                int active_client_fd = events[i].data.fd;
                int status = handle_client(active_client_fd);

                if(status == -1){
                    // CRITICAL FIX: Changed EPOLL_CTL_ADD to EPOLL_CTL_DEL
                    epoll_ctl(epoll_fd, EPOLL_CTL_DEL, active_client_fd, NULL);
                }
            }
        }
    }
    close(epoll_fd);
}


int main() {
    int server_fd;
    struct sockaddr_in server_addr;
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
    server_addr.sin_port = htons(PORT);       // Convert port to network byte order

    // 4. Bind the socket to the port and IP address
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // 5. Start listening for incoming connections
    if (listen(server_fd, 5) < 0) {
        perror("Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("TCP Server running on port %d...\n", PORT);

    // ========================================================
    // CHANGED: Simply trigger your event driven engine loop here.
    // ========================================================
    run_server_loop(server_fd);

    close(server_fd);
    return 0;
}
