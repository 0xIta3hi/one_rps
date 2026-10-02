# one_rps

A small C++ playground growing toward a lightweight TCP API.

This repository is currently the first stepping stone: a compact C++ exercise that demonstrates references, function overloading, and basic console output. The longer-term destination is a file-backed CRUD API, built up carefully from the networking layer.

## Current Status

**Stage 1: C++ fundamentals**

`main.cpp` currently demonstrates:

- Pass-by-value versus pass-by-reference with `change`
- Function overloading for `int`, `double`, and `std::string`
- Simple command-line output

`api.c` is reserved for the future API implementation and is currently empty.

## Example Output

Running the current program prints:

```text
10 200
int add called
double add called
string add called
```

The first line shows the key reference exercise: `a` is passed by value and remains `10`, while `b` is passed by reference and becomes `200`.

## Build and Run

### Requirements

- A C++ compiler with C++11 support or newer
- A POSIX-like shell for the commands below

### Compile

```bash
g++ -std=c++11 -Wall -Wextra -pedantic main.cpp -o main
```

### Run

```bash
./main
```

The executable named `main` is a build artifact. Rebuild it whenever `main.cpp` changes instead of relying on an older binary.

## Project Layout

```text
.
├── api.c       # Future API/TCP server implementation
├── main.cpp    # Current C++ practice program
└── main        # Locally built executable
```

## Roadmap

The project is intended to evolve in small, testable steps:

1. Build a simple TCP server in C++.
2. Add file-backed create, read, update, and delete operations.
3. Expose those operations through API endpoints.
4. Make the first performance target approximately 10,000 requests.
5. Investigate the architecture and operational work required for a much larger target of 1,000,000 requests per second.

The performance targets are goals for future iterations, not capabilities of the current program.

## Development Notes

Keep experiments focused and easy to run. A useful next increment is to introduce the TCP server behind a small, isolated interface before connecting persistence or request routing. That keeps the networking, storage, and API layers independently testable as the project grows.

## License

No license has been declared yet.
