# High-Concurrency TCP Server in C++

A single-threaded, event-driven TCP server written in C++ using Linux's `epoll` API.

The project demonstrates how nonblocking sockets and I/O multiplexing can be used to handle thousands of simultaneous TCP connections without creating a separate thread for each client.

## Features

- Event-driven networking using `epoll`
- Nonblocking TCP sockets
- Multiple concurrent client connections
- Per-client buffering for newline-delimited messages
- Graceful handling of client disconnections
- Built-in message throughput measurement
- Load-testing program supporting 10,000 concurrent clients
- Makefile for automated compilation and execution

## Requirements

- Linux
- GCC with C++17 support
- GNU Make
- Standard Linux networking headers
- Permission to set the open file descriptor limit to at least 20,000 for the 10,000-connection benchmark



## Building

### Using Make (Recommended)

Compile both the server and load tester:

```bash
make
```

This generates two executables:

- `server` — the TCP server
- `load_test` — the load-testing program

### Manual Compilation

Alternatively, compile the server:

```bash
g++ -std=c++17 -O2 -Wall -Wextra server.cpp -o server
```

Compile the load tester:

```bash
g++ -std=c++17 -O2 -Wall -Wextra load_test.cpp -o load_test
```

## Running

### 1. Start the server

Open a Linux terminal in the project directory and run:

```bash
make run-server
```

This command compiles the server if necessary, attempts to increase the open file descriptor limit to 20,000, and starts the server.

The server listens on TCP port `8080`.

### 2. Run the load tester

Open a second Linux terminal in the same project directory and run:

```bash
make run-test
```

This command compiles the load tester if necessary, attempts to increase its open file descriptor limit to 20,000, and starts the load test.

The load tester attempts to establish 10,000 TCP connections to `127.0.0.1:8080` and sends one newline-terminated message per connected client.

Press Enter in the load tester to close all client connections.

### File Descriptor Limits

Each TCP connection requires a file descriptor on both the server and the load-testing client.

To support 10,000 simultaneous connections, the Makefile uses:

```bash
ulimit -n 20000
```

This limit is applied separately to the server and load tester when launched through their respective Makefile targets.

The operating system must permit this limit. If the command fails, the user may need to adjust the Linux session's file descriptor limits before running the benchmark.

### Manual Execution

If not using the Makefile, run the following commands in the server terminal:

```bash
ulimit -n 20000
./server
```

Then, in a separate terminal:

```bash
ulimit -n 20000
./load_test
```

**Note:** The load tester and server are designed to run on the same Linux machine.

## Architecture

The server uses a single-threaded event loop:

1. Create a listening TCP socket and configure it as nonblocking.
2. Register the listening socket with an `epoll` instance.
3. Wait for socket readiness using `epoll_wait()`.
4. Accept incoming connections and register their sockets with `epoll`.
5. Read available client data using `recv()`.
6. Buffer incomplete messages and count complete newline-delimited messages.
7. Remove and close disconnected client sockets.

This architecture avoids the overhead of maintaining one thread per connection.

## Performance Testing

The project includes a load tester configured for:

- **10,000 concurrent TCP connections**
- **1 message per connection**
- **10,000 total messages**
- **Message throughput reporting**

### Benchmark Results

The server was benchmarked in an Ubuntu Linux virtual machine using the included C++ load-testing program.

| Metric | Result |
|---|---|
| Concurrent TCP connections | 10,000 |
| Messages received | 10,000 |
| Elapsed time | 2.41666 seconds |
| Message throughput | 4,137.95 messages/second |
| Networking model | Single-threaded `epoll` |
| Transport | TCP over localhost |

The server successfully maintained 10,000 simultaneous TCP connections and processed all 10,000 messages during the benchmark.

Throughput was measured using `std::chrono::steady_clock`, starting when the first complete message was received and ending when the 10,000th message was processed.

**Note:** These results represent a local benchmark conducted in an Ubuntu virtual machine. Performance may vary depending on hardware, operating system configuration, and workload. The measured throughput does not necessarily represent the server's maximum capacity.

## Makefile Commands

| Command | Description |
|---|---|
| `make` | Compile the server and load tester |
| `make run-server` | Set the file descriptor limit and start the server |
| `make run-test` | Set the file descriptor limit and start the load tester |
| `make clean` | Remove compiled executables |

## Technology

C++17 · Linux · TCP/IP · POSIX Sockets · epoll