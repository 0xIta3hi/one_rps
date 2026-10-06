# Benchmark Results

- This document records benchmark results for the high-performance TCP server as its architecture evolves.

- The purpose is not simply to track the highest requests/sec number, but to measure how each architectural change affects throughput and to understand why the performance changes.

# Test Environment
- OS: CachyOS
- CPU: 12 logical CPUs
- Server: Local TCP server
- Client: Custom multithreaded load generator
- Network: Loopback (127.0.0.1)
- Protocol: TCP
- Payload: PING\n
- Benchmark duration: 10 seconds
- Connections: 10 persistent TCP connections
- Server CPU affinity: CPUs 0-5
- Load generator CPU affinity: CPUs 6-11

The server and load generator are intentionally isolated onto different CPU sets to reduce contention between them.

# Benchmark Methodology

Each benchmark measures:
```
Total Requests
-----------------
Elapsed Time
```
giving:
```
Requests/sec = Total Requests / Elapsed Time
```

The same load generator and workload are used across architectural changes wherever possible.

The goal is to change one architectural variable at a time and compare the resulting throughput.

# Results

| Architecture             | Connections | Duration |  Requests |           Throughput |
| ------------------------ | ----------: | -------: | --------: | -------------------: |
| Blocking TCP server      |          10 |      10s | 1,086,248 |     108,624.80 req/s |
| Blocking + CPU isolation |          10 |      10s | 1,141,311 | **114,131.10 req/s** |
| Thread-per-connection    |          10 |      10s | 3,753,030 | **375,303.00 req/s** |

# 1. Blocking server
## Architecture

The original server handled one client completely before accepting another:

```
accept()
   ↓
read()
   ↓
write()
   ↓
read()
   ↓
write()
   ↓
...
   ↓
client disconnects
   ↓
accept next client
```
Only one client was actively handled by the server at a time.

### Result
```
Requests:   1,086,248
Duration:   10 seconds
Throughput: 108,624.80 req/s
```

# 2. Blocking server + CPU isolation
The server was pinned to CPU's 0-5 and load generator was pinned to 6-11.
### Results
```
Requests:   1,141,311
Duration:   10 seconds
Throughput: 114,131.10 req/s
```
### Improvement
Compared with the previous 108,624.80 req/s result that is an 5.1% improvement which showed that contention between the server and load generator was contributing to the benchmark.but CPU contention was not the primary bottleneck.

# 3. Thread-Per-Connection Server
The server architecture was changed so that every accepted connection gets its own worker thread.
```
                  ┌── Worker 1 → Client 1
                  │
accept() ─────────┼── Worker 2 → Client 2
                  │
                  ├── Worker 3 → Client 3
                  │
                  └── Worker N → Client N
```
The main thread is responsible for accepting connections while worker threads independently perform the client read/write loop.

### Improvement
Compared with the CPU-isolated blocking server the architecture therefore produced approximately a 3.29× throughput improvement.

# Observation
The major performance improvement came from changing the concurrency model rather than from low-level optimization.
### Blocking model
```
One server thread
      ↓
One active client
      ↓
Blocking I/O
      ↓
Next client
```
A connection could prevent the server from making progress on other connections.

### Threaded Model
```
Main thread
      ↓
accept()
      ↓
create worker
      ↓
immediately accept another client
```
Multiple connections can now make progress concurrently.
This demonstrates an important systems principle:

> A system can be limited by its concurrency model even when the CPU is not fully utilized.

# Current Performance target
The long-term target: 1M requests/sec.                      
Current Best: 375,303 requests/sec      
which shows that current model still need to improve by a factor of 2.66x to reach the original target.
