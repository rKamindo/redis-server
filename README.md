# Redis Server in C

This project is a Redis server implementation in C, created as part of the [Coding Challenges](https://codingchallenges.fyi/challenges/challenge-redis) project.

## Features

- RESP (REdis Serialization Protocol) implementation

## Build Instructions

This project uses CMake for building. GoogleTest is used for unit testing.

```bash
mkdir build
cmake -S . -B build
cmake --build build
```

## Running Tests

After building the project, you can run the tests using CTest, CMake's testing tool. Follow these steps:

```bash
cd build
ctest
```
