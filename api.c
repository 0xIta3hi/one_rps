#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
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

// Client structure
struct client {
    int fd;
    char output_buffer[BUFFER_SIZE];
    size_t output_length;
    size_t output_sent;
};
int flush_client_output(struct client *client)
{
    while (client->output_sent < client->output_length) {

        ssize_t bytes_written = write(
            client->fd,
            client->output_buffer + client->output_sent,
            client->output_length - client->output_sent
        );

        if (bytes_written > 0) {
            client->output_sent += bytes_written;
            continue;
        }

        if (bytes_written < 0 && errno == EINTR) {
            continue;
        }

        if (bytes_written < 0 &&
            (errno == EAGAIN || errno == EWOULDBLOCK)) {
            return 0;
        }

        return -1;
    }

    // All pending bytes have been written.
    client->output_length = 0;
    client->output_sent = 0;

    return 1;
}

int modify_client_events(int epoll_fd, struct client *client, int want_output)
{
    struct epoll_event ev;

    ev.events = EPOLLIN | EPOLLET;
    if (want_output) {
        ev.events |= EPOLLOUT;
    }
    ev.data.ptr = client;

    return epoll_ctl(epoll_fd, EPOLL_CTL_MOD, client->fd, &ev);
}

int handle_client(struct client *client)
{
    char buffer[BUFFER_SIZE];

    while (client->output_length == 0) {
        ssize_t bytes_read = read(client->fd, buffer, sizeof(buffer));

        if (bytes_read < 0) {
            if (errno == EINTR) {
                continue;
            }
            if (errno == EAGAIN || errno == EWOULDBLOCK) {
                return 0;
            }
            perror("Read Error");
            return -1;
        }

        if (bytes_read == 0) {
            return -1;
        }

        // Keep input in a temporary buffer until the client's output is available.
        memcpy(client->output_buffer, buffer, (size_t)bytes_read);
        client->output_length = (size_t)bytes_read;
        client->output_sent = 0;

        if (flush_client_output(client) < 0) {
            return -1;
        }

        // If the write would block, the event loop will resume input after EPOLLOUT.
        if (client->output_length != 0) {
            return 0;
        }
    }

    return 0;
}

void run_server_loop(int server_fd){
    int epoll_fd = epoll_create1(0);
    if (epoll_fd < 0) {
        perror("epoll_create1 failed");
        return;
    }
    
    struct epoll_event ev, events[MAX_EVENTS];

    // Using Edge-Triggered (EPOLLET) for the non-blocking listening socket
    ev.events = EPOLLIN | EPOLLET;
    ev.data.ptr = NULL;
    epoll_ctl(epoll_fd, EPOLL_CTL_ADD, server_fd, &ev);

    while(1){
        int nfds = epoll_wait(epoll_fd, events, MAX_EVENTS, -1);
        if (nfds < 0) {
            if (errno == EINTR) continue;
            perror("epoll_wait failed");
            break;
        }

        for(int i = 0; i < nfds; ++i){
            struct client *client = events[i].data.ptr;

            if (client == NULL) {
                // Drain the listen queue because the listening socket is edge-triggered.
                while (1) {
                    struct sockaddr_in client_addr;
                    socklen_t addr_len = sizeof(client_addr);
                    
                    int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
                    if(client_fd < 0){
                        // Out of incoming client connections to accept right now
                        if (errno == EAGAIN || errno == EWOULDBLOCK) {
                            break; 
                        }
                        perror("Accept failed");
                        break;
                    }
                    
                    if (set_non_blocking(client_fd) < 0) {
                        perror("Failed to set client socket to non-blocking");
                        close(client_fd);
                        continue;
                    }

                    client = calloc(1, sizeof(*client));
                    if (client == NULL) {
                        perror("calloc failed");
                        close(client_fd);
                        continue;
                    }
                    client->fd = client_fd;
                    
                    ev.events = EPOLLIN | EPOLLET;
                    ev.data.ptr = client;
                    if (epoll_ctl(epoll_fd, EPOLL_CTL_ADD, client_fd, &ev) < 0) {
                        perror("Failed to add client to epoll");
                        close(client_fd);
                        free(client);
                        continue;
                    }
                    
                    printf("New client connected from %s:%d (fd %d)\n", 
                           inet_ntoa(client_addr.sin_addr), ntohs(client_addr.sin_port), client_fd);
                }
                continue;
            }

            int close_client = 0;

            if (events[i].events & (EPOLLERR | EPOLLHUP)) {
                close_client = 1;
            } else if (events[i].events & EPOLLOUT) {
                int status = flush_client_output(client);

                if (status < 0) {
                    close_client = 1;
                } else if (status == 1) {
                    // Stop asking for writable events, then drain input already queued.
                    if (modify_client_events(epoll_fd, client, 0) < 0 ||
                        handle_client(client) < 0) {
                        close_client = 1;
                    } else if (client->output_length != 0 &&
                               modify_client_events(epoll_fd, client, 1) < 0) {
                        close_client = 1;
                    }
                }
            } else if (events[i].events & EPOLLIN) {
                if (handle_client(client) < 0) {
                    close_client = 1;
                }
            }

            if (!close_client && client->output_length != 0 &&
                modify_client_events(epoll_fd, client, 1) < 0) {
                close_client = 1;
            }

            if (close_client) {
                // All client ownership ends here, preventing double close/free paths.
                epoll_ctl(epoll_fd, EPOLL_CTL_DEL, client->fd, NULL);
                close(client->fd);
                free(client);
            }
        }
    }
    close(epoll_fd);
}


int main() {
    int server_fd;
    struct sockaddr_in server_addr;
    int opt = 1;

    // 1. Create socket file descriptor
    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    // 2. Set socket options to avoid "Address already in use" errors
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        perror("setsockopt failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // CHANGE 1: Make the primary server listening socket non-blocking
    if (set_non_blocking(server_fd) < 0) {
        perror("Failed to set server socket to non-blocking");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // 3. Define the server address structure
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY; 
    server_addr.sin_port = htons(PORT);       

    // 4. Bind the socket
    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    // 5. Start listening for incoming connections
    if (listen(server_fd, 128) < 0) { // Bumped backlog space from 5 to 128 for non-blocking burst traffic
        perror("Listen failed");
        close(server_fd);
        exit(EXIT_FAILURE);
    }

    printf("TCP Server running on port %d with non-blocking edge triggers...\n", PORT);

    run_server_loop(server_fd);

    close(server_fd);
    return 0;
}
