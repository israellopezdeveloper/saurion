# Saurion

[![Quality Gate Status](https://sonarcloud.io/api/project_badges/measure?project=israellopezdeveloper_saurion&metric=alert_status)](https://sonarcloud.io/summary/new_code?id=israellopezdeveloper_saurion)
[![CI Main](https://github.com/israellopezdeveloper/saurion/actions/workflows/main.yml/badge.svg)](https://github.com/israellopezdeveloper/saurion/actions/workflows/main.yml)
![Saurion](https://raw.githubusercontent.com/israellopezdeveloper/saurion/refs/heads/metadata-branch/logo.png)

## Table of Contents

<!--toc:start-->

- [Saurion](#saurion)
  - [Table of Contents](#table-of-contents)
  - [Overview](#overview)
  - [Message Structure](#message-structure)
    - [Example of a 9000-byte Message](#example-of-a-9000-byte-message)
      - [Message Layout](#message-layout)
      - [`iovec` Division](#iovec-division)
  - [Design Diagrams](#design-diagrams)
    - [General Architecture](#general-architecture)
    - [Thread Pool](#thread-pool)
  - [Usage Examples](#usage-examples)
    - [Basic I/O Handling](#basic-io-handling)
    - [Thread Pool Example](#thread-pool-example)
  - [Modules](#modules)
    - [Thread Pool Module](#thread-pool-module)
      - [Thread Pool Key Functions](#thread-pool-key-functions)
      - [Thread Pool Diagram](#thread-pool-diagram)
    - [Linked List Module](#linked-list-module)
      - [Linked List Key Functions](#linked-list-key-functions)
      - [Linked List Diagram](#linked-list-diagram)
    - [Saurion Module](#saurion-module)
      - [Saurion Key Features](#saurion-key-features)
  - [Building and Running](#building-and-running)
  <!--toc:end-->

[Web Page](https://israellopezdeveloper.github.io/saurion/)

---

## Overview

The **Saurion** library is designed to efficiently handle asynchronous
input/output events on Linux systems using the `io_uring` API. It enhances
performance and scalability in highly concurrent applications, making it ideal
for network operations such as socket connections, reads, writes, and closures.

The primary `saurion` structure encapsulates `io_uring` rings and manages
synchronization across threads via a thread pool. This design enables
high-performance I/O handling for multiple sockets simultaneously without
blocking threads during operations.

---

## Message Structure

Each message consists of three components:

1. **Header**: An unsigned 64-bit integer representing the length of the
   message body.
2. **Body**: The actual data of the message.
3. **Footer**: A single byte set to `0`.

When sent to the kernel, messages are divided into chunks using `iovec` for
efficient transmission. Each `iovec` has:

- `iov_base`: Array where the chunk is stored.
- `iov_len`: Number of bytes in `iov_base`.

### Example of a 9000-byte Message

#### Message Layout

```text
+------------------+--------------------+----------+
|    Header        |       Body         |  Footer  |
|  (64 bits: 9000) |   (Message Data)   | (1 byte) |
+------------------+--------------------+----------+
```

#### `iovec` Division

1. **First `iovec`** (8192 bytes):

   - 8 bytes header.
   - 8184 bytes of message body.
   - `iov_len = 8192`.

2. **Second `iovec`** (817 bytes):
   - 816 bytes of message body.
   - 1 byte footer.
   - `iov_len = 817`.

---

## Design Diagrams

### General Architecture

```text
+---------------------+
|      Saurion        |
+---------+-----------+
          |
   +------v-------+          +---------+
   | Thread Pool   |<--------| Task    |
   +------+--------+         +---------+
          |
   +------v-------+
   |  io_uring     |
   +------+--------+
          |
   +------v-------+
   | Linked List   |
   +--------------+
```

### Thread Pool

```text
Threads: [T1, T2, T3, T4]
Task Queue: [Task1] -> [Task2] -> NULL
```

---

## Usage Examples

### Basic I/O Handling

```c
#include "saurion.h"

struct saurion *s = saurion_create(4);

if (saurion_start(s) != 0) {
    // Handle the error
}

saurion_send(s, socket_fd, "Hello, Saurion!");

saurion_stop(s);
saurion_destroy(s);
```

### Thread Pool Example

```c
#include "threadpool.h"

void example_task(void *arg) {
    printf("Task executed with arg: %d\n", *(int *)arg);
}

int main() {
    struct threadpool *pool = threadpool_create_default();
    threadpool_init(pool);

    int task_arg = 42;
    threadpool_add(pool, example_task, &task_arg);

    threadpool_wait_empty(pool);
    threadpool_destroy(pool);
    return 0;
}
```

---

## Modules

### Thread Pool Module

The **Thread Pool** module manages a fixed number of threads and a task queue.
It ensures efficient task execution and synchronization.

#### Thread Pool Key Functions

- `threadpool_create` creates a thread pool.
- `threadpool_add` adds a task to the queue.
- `threadpool_wait_empty` blocks until all tasks are complete.

#### Thread Pool Diagram

```text
Threads: [T1, T2, T3]
Task Queue: [Task1] -> [Task2] -> NULL
```

---

### Linked List Module

The **Linked List** module provides thread-safe node management for dynamically
growing lists.

#### Linked List Key Functions

- `list_insert` adds nodes.
- `list_delete_node` removes specific nodes.
- `list_free` cleans up all nodes.

#### Linked List Diagram

```text
Head -> [Node(ptr=A)] -> [Node(ptr=B)] -> NULL
```

---

### Saurion Module

**Saurion** leverages `io_uring` for asynchronous I/O, allowing non-blocking
operations on sockets.

#### Saurion Key Features

- Custom callbacks for connection, read, write, close, and error events.
- Efficient message chunking with `iovec`.
- Multi-threaded processing via a thread pool.

---

## Building and Running

1. Clone the repository:

   ```bash
   git clone https://github.com/israellopezdeveloper/saurion.git
   cd saurion
   ```

2. Build the project:

   ```bash
   make
   ```

3. Run tests:

   ```bash
   make test
   ```

4. Use `doxygen` for documentation:

   ```bash
   doxygen Doxyfile
   ```
