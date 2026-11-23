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
#include <errno.h>

// --- CONFIGURATION ---
#define SERVER_PORT 8080
#define SERVER_IP "127.0.0.1"
#define NUM_THREADS 100         // Concurrent threads
#define RUN_TIME_SECONDS 10.0   // Benchmark duration
#define PAYLOAD_SIZE (4 * 1024)

// --- GLOBAL STATE ---
atomic_long global_request_count = 0;
atomic_bool keep_running = true;

// --- SYNCHRONIZATION STATE ---
pthread_mutex_t ready_mtx = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t ready_cond = PTHREAD_COND_INITIALIZER;
int checked_in_threads = 0; // Total threads that finished setup (connected OR failed)
int connected_threads = 0;  // Threads actually ready to send load
bool start_benchmark = false;

int send_all(int sockfd, const void *buf, size_t len) {
    size_t total_sent = 0;
    const char *ptr = (const char *)buf;
    while (total_sent < len) {
        ssize_t n = send(sockfd, ptr + total_sent, len - total_sent, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) return -1;
        total_sent += (size_t)n;
    }
    return 0;
}

int recv_all(int sockfd, void *buf, size_t len) {
    size_t total_recvd = 0;
    char *ptr = (char *)buf;

    while (total_recvd < len) {
        ssize_t n = recv(sockfd, ptr + total_recvd, len - total_recvd, 0);
        if (n < 0) {
            if (errno == EINTR) continue;
            return -1;
        }
        if (n == 0) return -1;
        total_recvd += (size_t)n;
    }
    return 0;
}

// Helper to notify main thread when a worker fails setup early (prevents deadlock)
static void report_setup_failure(void) {
    pthread_mutex_lock(&ready_mtx);
    checked_in_threads++;
    if (checked_in_threads == NUM_THREADS) {
        pthread_cond_broadcast(&ready_cond);
    }
    pthread_mutex_unlock(&ready_mtx);
}

// Thread worker function
void* thread_worker(void* arg) {
    int thread_id = *(int*)arg;
    free(arg);

    int sock = 0;
    struct sockaddr_in serv_addr;
    memset(&serv_addr, 0, sizeof(serv_addr));

    // 1. Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        printf("[Thread %d] Socket creation error\n", thread_id);
        report_setup_failure();
        return NULL;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &serv_addr.sin_addr);

    // 2. Connect to the server
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        printf("[Thread %d] Connection Failed!\n", thread_id);
        close(sock);
        report_setup_failure();
        return NULL;
    }

    char request[PAYLOAD_SIZE];
    char buffer[PAYLOAD_SIZE];
    memset(request, 'A', sizeof(request));

    // 3. Synchronize: signal readiness and wait for main thread's start signal
    pthread_mutex_lock(&ready_mtx);
    checked_in_threads++;
    connected_threads++;
    if (checked_in_threads == NUM_THREADS) {
        pthread_cond_broadcast(&ready_cond); // Wake main thread
    }
    while (!start_benchmark) {
        pthread_cond_wait(&ready_cond, &ready_mtx);
    }
    pthread_mutex_unlock(&ready_mtx);

    // 4. Keep sending requests until the main thread tells everyone to stop
    while (atomic_load_explicit(&keep_running, memory_order_relaxed)) {
        if (send_all(sock, request, sizeof(request)) < 0) break;
        if (recv_all(sock, buffer, sizeof(buffer)) < 0) break;

        // Atomically increment the total request counter (thread-safe)
        atomic_fetch_add_explicit(&global_request_count, 1, memory_order_relaxed);
    }

    close(sock);
    return NULL;
}

int main() {
    pthread_t threads[NUM_THREADS];
    struct timespec start, current, end;

    printf("Spawning %d workers | Target Duration: %.1f seconds\n", NUM_THREADS, RUN_TIME_SECONDS);

    // 1. Spawn the worker threads
    for (int i = 0; i < NUM_THREADS; i++) {
        int *id = malloc(sizeof(int));
        *id = i + 1;
        if (pthread_create(&threads[i], NULL, thread_worker, id) != 0) {
            perror("Failed to create thread");
            return 1;
        }
    }

    // 2. Wait for all workers to finish TCP connect() and reach the gate
    pthread_mutex_lock(&ready_mtx);
    while (checked_in_threads < NUM_THREADS) {
        pthread_cond_wait(&ready_cond, &ready_mtx);
    }

    if (connected_threads == 0) {
        fprintf(stderr, "Error: No workers connected to %s:%d. Aborting.\n", SERVER_IP, SERVER_PORT);
        start_benchmark = true;
        pthread_cond_broadcast(&ready_cond);
        pthread_mutex_unlock(&ready_mtx);
        return 1;
    }

    printf("All %d/%d workers connected and ready. Starting timer!\n\n", connected_threads, NUM_THREADS);

    // 3. Record start time BEFORE releasing workers
    clock_gettime(CLOCK_MONOTONIC, &start);
    start_benchmark = true;
    pthread_cond_broadcast(&ready_cond);
    pthread_mutex_unlock(&ready_mtx);

    // 4. Main thread acts as the referee/timer
    struct timespec req = {
        .tv_sec = 0,
        .tv_nsec = 10000000L // 10 ms
    };

    while (true) {
        clock_gettime(CLOCK_MONOTONIC, &current);
        double elapsed_seconds = (double)(current.tv_sec - start.tv_sec) +
                                 (double)(current.tv_nsec - start.tv_nsec) / 1e9;

        if (elapsed_seconds >= RUN_TIME_SECONDS) {
            // Time's up! Signal threads to stop looping
            atomic_store_explicit(&keep_running, false, memory_order_relaxed);
            break;
        }
        nanosleep(&req, NULL);
    }

    // 5. Wait for all worker threads to finish in-flight requests and exit
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }

    // Measure true total elapsed time including final in-flight drain
    clock_gettime(CLOCK_MONOTONIC, &end);
    double total_elapsed = (double)(end.tv_sec - start.tv_sec) +
                           (double)(end.tv_nsec - start.tv_nsec) / 1e9;

    long long total_requests = atomic_load(&global_request_count);
    double requests_per_second = (double)total_requests / total_elapsed;

    printf("--- Benchmark Complete ---\n");
    printf("Active Connections: %d\n", connected_threads);
    printf("Total Elapsed Time: %.4f seconds\n", total_elapsed);
    printf("Total Requests Sent: %lld\n", total_requests);
    printf("Throughput:         %.2f requests/sec\n", requests_per_second);

    return 0;
}