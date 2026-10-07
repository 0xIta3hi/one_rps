#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <pthread.h>
#include <stdatomic.h>

// --- CONFIGURATION ---
#define SERVER_PORT 8080
#define SERVER_IP "127.0.0.1"
#define NUM_THREADS 5        // <--- How many concurrent connections you want
#define RUN_TIME_SECONDS 10.0   // <--- Benchmark duration

// --- GLOBAL STATE ---
atomic_long global_request_count = 0;
atomic_bool keep_running = true;

// Thread worker function
void* thread_worker(void* arg) {
    int thread_id = *(int*)arg;
    free(arg);

    int sock = 0;
    struct sockaddr_in serv_addr;

    // 1. Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("[Thread %d] Socket creation error\n", thread_id);
        return NULL;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr);

    // 2. Connect to the server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("[Thread %d] Connection Failed!\n", thread_id);
        close(sock);
        return NULL;
    }

    const char *request = "PING\n";
    char buffer[1024];

    // 3. Keep sending requests until the main thread tells everyone to stop
    while (atomic_load(&keep_running)) {
        if (send(sock, request, strlen(request), 0) < 0) break;
        if (recv(sock, buffer, sizeof(buffer) - 1, 0) <= 0) break;

        // Atomically increment the total request counter (thread-safe)
        atomic_fetch_add(&global_request_count, 1);
    }

    close(sock);
    return NULL;
}

int main() {
    pthread_t threads[NUM_THREADS];
    struct timespec start, current;

    printf("Starting multi-threaded benchmark...\n");
    printf("Connections: %d | Target Duration: %.1f seconds\n\n", NUM_THREADS, RUN_TIME_SECONDS);

    // 1. Record start time
    clock_gettime(CLOCK_MONOTONIC, &start);

    // 2. Spawn the worker threads
    for (int i = 0; i < NUM_THREADS; i++) {
        int *id = malloc(sizeof(int));
        *id = i + 1;
        if (pthread_create(&threads[i], NULL, thread_worker, id) != 0) {
            perror("Failed to create thread");
            return 1;
        }
    }

    // 3. Main thread acts as the referee/timer
    while (true) {
        clock_gettime(CLOCK_MONOTONIC, &current);
        double elapsed_seconds = (double)(current.tv_sec - start.tv_sec) + 
                                 (double)(current.tv_nsec - start.tv_nsec) / 1000000000.0;

        if (elapsed_seconds >= RUN_TIME_SECONDS) {
            // Time's up! Signal threads to stop looping
            atomic_store(&keep_running, false);

            // Wait for all worker threads to finish cleaning up and exit
            for (int i = 0; i < NUM_THREADS; i++) {
                pthread_join(threads[i], NULL);
            }

            // Calculate metrics using the atomic total
            long long total_requests = atomic_load(&global_request_count);
            double requests_per_second = (double)total_requests / elapsed_seconds;

            printf("--- Benchmark Complete ---\n");
            printf("Total Elapsed Time: %.4f seconds\n", elapsed_seconds);
            printf("Total Requests Sent: %lld\n", total_requests);
            printf("Throughput:         %.2f requests/sec\n", requests_per_second);
            break;
        }
        
        // Sleep for a fraction of a second so the main thread doesn't hog the CPU checking the time
    }

    return 0;
}
