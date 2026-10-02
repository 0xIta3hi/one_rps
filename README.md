# one_rps

A small learning project focused on building a lightweight API/server from the ground up.

This repository is in its early stages: it currently contains a C++ practice program and a blank file reserved for a future TCP/API implementation. The long-term direction is to evolve this project into a simple file-backed CRUD service with a minimal networking layer and a clear performance roadmap.

## Project status

Right now, the repo is best described as a prototype and learning scaffold rather than a running API service.

### Current state

- `main.cpp` demonstrates:
  - pass-by-reference behavior
  - function overloading for `int`, `double`, and `std::string`
  - basic console output
- `api.c` is intentionally empty and reserved for future TCP/server work
- `main` is a compiled binary generated locally from `main.cpp`

## What it is trying to become

The project notes in `main.cpp` outline a progression like this:

1. Build a simple TCP server in C++
2. Add a temporary file-based database
3. Implement CRUD operations on top of that data layer
4. Expose endpoints for API-style operations
5. Scale toward handling approximately 10,000 requests
6. Explore the architecture needed for much larger throughput targets, including 1M requests per second

This is a hands-on coding exercise and a roadmap for gradual system design, not a production-ready service yet.

## Quick start

### Requirements

- A C++ compiler supporting C++11 or newer
- A Unix-like shell environment

### Compile

```bash
g++ -std=c++11 -Wall -Wextra -pedantic main.cpp -o main
```

### Run

```bash
./main
```

### Expected output

```text
10 200
int add called
double add called
string add called
```

The first line confirms the reference behavior: the value passed by value stays unchanged, while the reference parameter updates.

## Repository layout

```text
.
├── api.c       # Future TCP/API server work
├── main.cpp    # Current C++ learning/demo file
├── main        # Local compiled executable
├── README.md   # Project overview and instructions
```

## Development notes

The structure is intentionally simple so each layer can be added incrementally:

- keep networking concerns separate from storage logic
- keep CRUD behavior isolated and easy to test
- build in small steps instead of jumping straight to performance tuning

This makes the project a good practice ground for learning how to grow a service from a minimal starting point.

## Roadmap

### Near-term goals

- build the TCP server foundation
- define a minimal request/response format
- add file-backed persistence for basic record operations

### Mid-term goals

- add CRUD API endpoints
- support concurrency and basic validation
- test throughput under moderate request volumes

### Long-term goals

- optimize architecture for higher throughput
- measure bottlenecks and tune design decisions
- revisit whether a file-based model is adequate or if a database is needed

## License

No license has been declared for this project yet.
