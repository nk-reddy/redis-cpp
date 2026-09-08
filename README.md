# Redis Clone in C++

A Redis clone implemented in C++ as part of the codecrafters challenge. Implements a TCP server that speaks RESP, 
maintains per-client state, stores data in memory, and supports many Redis functions.

## Features

- TCP server through posix sockets
- RESP command parsing and response encoding
- Concurrent client handling with threads
- In-memory key/value store with multiple value types
- Key expiration
- Transactions with `MULTI`, `EXEC`, `DISCARD`, `WATCH`, and `UNWATCH`
- Primary/replica replication and command propagation
- RDB file loading
- AOF persistence and replay
- Pub/Sub
- Redis streams
- Sorted sets
- Geospatial commands
- Bitmap operations
- ACL-based authentication

## Architecture

```text
src/
├── main.cpp
├── client.cpp
├── client-state/
├── cli/
├── commands/
├── replication/
├── server/
└── store/
```

## Building

### Requirements

- C++23 compiler
- CMake
- pthreads
- standalone Asio
- OpenSSL

Build with:

```bash
cmake -B build
cmake --build build
```

The executable will be created at:

```bash
./build/redis
```

## Running

Start the server on the default Redis port:

```bash
./build/redis
```

Then connect with the standard Redis CLI:

```bash
redis-cli
```

Example:

```text
127.0.0.1:6379> SET greeting hello
OK

127.0.0.1:6379> GET greeting
"hello"
```

### Custom port

Start the server on a different port:

```bash
./build/redis --port 6380
```

Connect with:

```bash
redis-cli -p 6380
```

### Start a replica

Start the primary:

```bash
./build/redis --port 6379
```

Start another instance as its replica:

```bash
./build/redis --port 6380 --replicaof localhost 6379
```

### Load an RDB file

```bash
./build/redis --dir /path/to/data --dbfilename dump.rdb
```

## Example: Transactions

```text
127.0.0.1:6379> MULTI
OK

127.0.0.1:6379> SET name redis
QUEUED

127.0.0.1:6379> GET name
QUEUED

127.0.0.1:6379> EXEC
1) OK
2) "redis"
```

`WATCH` is also supported to detect modifications to watched keys before a transaction executes.
