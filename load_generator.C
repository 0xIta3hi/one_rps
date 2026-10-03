#include<stdio.h>
#include<time.h>
#include<stdbool.h>
#include<unistd.h>
#include<arpa/inet.h>
#include<netinet/in.h>
#include<sys/socket.h>
#include<string.h>

bool send_request(int sock){
    char *request = "GET / HTTP/1.1\r\nHost: localhost\r\n\r\n";
    char buf[1024] = {0};
    if(send(sock, request, strlen(request), 0) < 0){
        return false;
    }

    if(recv(sock, buf, sizeof(buf),0) < 0){
        return false;
    }
    return true;
    usleep(1);
}


int main(){
    int sock = 0;
    struct sockaddr_in serv_addr;

    if((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0){
        printf("Socket creation error");
        return -1;
    }
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(8080);

    if(inet_pton(AF_INET, "127.0.0.1", &serv_addr.sin_addr) <= 0) {
        printf("invalid address");
        close(sock);
        return -1;
    }

    printf("Connecting to the server... \n");

    if(connect(sock, (struct server_addr*)&serv_addr, sizeof(serv_addr)) < 0){
        printf("Connection Failed !");
        close(sock);
        return -1;
    }

    printf("Conencted successfully !");

    struct timespec start, current;
    long long request_count = 0;
    double target_runtime_seconds = 5.0; // Run for 5 seconds

    printf("Starting benchmark for %.1f seconds...\n", target_runtime_seconds);
    clock_gettime(CLOCK_MONOTONIC, &start);

    while (true) {
        if (send_request(sock)) {
            request_count++;
        } else {
            printf("\n[Error] Connection broken mid-benchmark!\n");
            break;
        }

        // Check if runtime has expired
        clock_gettime(CLOCK_MONOTONIC, &current);
        double elapsed_seconds = (double)(current.tv_sec - start.tv_sec) + 
                                 (double)(current.tv_nsec - start.tv_nsec) / 1000000000.0;

        if (elapsed_seconds >= target_runtime_seconds) {
            double requests_per_second = (double)request_count / elapsed_seconds;

            printf("\n--- Benchmark Complete ---\n");
            printf("Total Elapsed Time: %.4f seconds\n", elapsed_seconds);
            printf("Total Requests Sent: %lld\n", request_count);
            printf("Throughput:         %.2f requests/sec\n", requests_per_second);
            break;
        }
    }

    // Clean up and close the socket before exiting
    close(sock);
    return 0;

}