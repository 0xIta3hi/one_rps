#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define PORT 8080
#define BACKLOG 128
#define BUFFER_SIZE 4096

static void die(const char *msg) {
    perror(msg);
    exit(EXIT_FAILURE);
}

int main(void) {
    // 1. Create the listening socket
    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd < 0) {
        die("socket");
    }

    // Allow immediate port reuse after server restarts (avoids TIME_WAIT errors)
    int opt = 1;
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        die("setsockopt");
    }

    // 2. Bind socket to 0.0.0.0:8080
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        die("bind");
    }

    // 3. Mark socket as passive and set kernel accept queue size
    if (listen(server_fd, BACKLOG) < 0) {
        die("listen");
    }

    printf("one_rps v0.1 (blocking TCP echo) listening on port %d...\n", PORT);

    // 4. Main server loop: accept one client at a time
    while (1) {
        struct sockaddr_in client_addr;
        socklen_t client_len = sizeof(client_addr);

        int client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_len);
        if (client_fd < 0) {
            perror("accept");
            continue;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
        printf("Client connected: %s:%d (fd=%d)\n",
               client_ip, ntohs(client_addr.sin_port), client_fd);

        // 5. Echo loop: keep reading until this client disconnects
        char buffer[BUFFER_SIZE];
        while (1) {
            ssize_t bytes_read = read(client_fd, buffer, sizeof(buffer));
            if (bytes_read == 0) {
                // EOF: client closed the connection cleanly
                printf("Client disconnected (fd=%d)\n", client_fd);
                break;
            }
            if (bytes_read < 0) {
                perror("read");
                break;
            }

            // Echo exact bytes back (naive write; we'll fix partial writes in Article 06)
            if (write(client_fd, buffer, bytes_read) < 0) {
                perror("write");
                break;
            }
        }

        close(client_fd);
    }

    close(server_fd);
    return 0;
}
