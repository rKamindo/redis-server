# Redis Server in C

This is a high-performance in-memory data store like Redis, that is compatible with Redis's RESP protocol. I've created it as part of the [Coding Challenges](https://codingchallenges.fyi/challenges/challenge-redis) project.

## Latest benchmark results
![image](https://github.com/user-attachments/assets/fc269633-df65-4440-a3f3-7e1abd69aacb)
![image](https://github.com/user-attachments/assets/ca8177ca-0559-4318-8136-693a8af28ef9)

## Features
- RESP (REdis Serialization Protocol) implementation
- Support for the following Redis commands (PING, ECHO, SET (with options), GET, EXIST, DELETE, INCR, DECR, LPUSH, LRANGE, RPUSH, SAVE)
- Persistence 
- Replication


## What I'm currently working on
- Replication


## Technical Implementation 
- epoll-based I/O multiplexing
- Ring buffers (input and output) for each client
- Zero allocation byte parsing for RESP protocol
- Asynchronous replication supporting partial resynchronization using a backlog

## System Requirements
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
127.0.0.1:6379> SET key value
OK
127.0.0.1:6379> GET key
"value"
```
## Using SET with Options
The SET command in this Redis server implementation supports additional options for enhanced functionality:

The syntax is:
```
SET key value [NX | XX] [GET] [EX seconds | PX milliseconds |
  EXAT unix-time-seconds | PXAT unix-time-milliseconds | KEEPTTL]
```

### Key Expiry
You can set an expiration time for a key using various key expiration options:
 - EX: Set the expiration time in seconds.
 - PX: Set the expiration time in milliseconds.
 - EXAT: Set the expiration time to a specific Unix timestamp (in seconds).
 - PXAT: Set the expiration time to a specific Unix timestamp (in milliseconds).
 - KEEPTTL: Retain the existing TTL (time-to-live) of the key.

### Using GET Option
The GET option allows you to return the old value stored at the key while setting a new value. If the key does not exist, it will return nil.

Using EX:
```bash
127.0.0.1:6379> SET mykey "value" EX 60  # Key expires after 60 seconds
OK
```
Using PX:
```bash
127.0.0.1:6379> SET anotherkey "value1" PX 500  # Key expires after 500 milliseconds
OK
```
Using EXAT:
```bash
127.0.0.1:6379> SET mykey2 "value2" EXAT 1672531199  # Key expires at specific Unix timestamp
OK
```
Using PXAT:
```bash
127.0.0.1:6379> SET mykey3 "value3" PXAT 1672531199000  # Key expires at specific Unix timestamp in milliseconds
OK
```
Using KEEPTTL:
```bash
127.0.0.1:6379> SET mykey4 "value4" KEEPTTL  # Retain the existing TTL of mykey4
OK
```
Using GET:
```bash
127.0.0.1:6379> SET cool_key cool_value GET # SET cool_key to cool_value and return previous value if it existed
nil # Since cool_key did not exist before, the previous value is nil.
127.0.0.1:6379> SET cool_key another_value # SET cool_key to cool_value and return previous value if it existed
cool_value
```

### Conditional Setting with NX and XX
NX: Only set the key if it does not already exist.
XX: Only set the key if it already exists.

## Running Tests

After building the project, you can run the tests using CTest, CMake's testing tool. Follow these steps:

```bash
cd build
ctest
```
