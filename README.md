# Redis Server in C

This ongoing project is a lightweight Redis compatible server implementation in C, created as part of the [Coding Challenges](https://codingchallenges.fyi/challenges/challenge-redis) project.

Currently working on improving the performance of this server using a variety of techniques from introducing a state machine to efficiently parse messages, to using separate input and output buffers for each client, all in all to allow better concurrent handling of requests.

## Features
- RESP (REdis Serialization Protocol) implementation
- Support for basic Redis commands (PING, ECHO, SET (no expiry options), GET)
- GoogleTest for unit testing
- Cmake for building

# System Requirements
- Linux operating system (or other Unix-like systems)
- CMake (version 3.10 or higher)cmake
- GCC or Clang compiler with C11 support

## Build Instructions
This project uses CMake for building.

```bash
mkdir build
cmake -S . -B build
cmake --build build
```

## Running the server
After building the project, you can run the server with the following command
```bash
./build/redis-lite
```
The server will start listening on port 6379 by default.

Once the server is running, you can use `redis-cli` to test various commands.

Start the redis-cli, and connect to the server.
```bash
redis-cli
127.0.0.1:6379> PING
PONG
127.0.0.1:6379> ECHO "Hello World"
"Hello World"
127.0.0.1:6379> SET Name Randy
OK
127.0.0.1:6379> GET Name
Randy
127.0.0.1:6379> SET Count 1
OK
127.0.0.1:6379> GET Count
1
```

## Running Tests

After building the project, you can run the tests using CTest, CMake's testing tool. Follow these steps:

```bash
cd build
ctest
```
