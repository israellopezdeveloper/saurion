# Saurion

<!--toc:start-->

- [Saurion](#saurion)
<!--toc:end-->

[![Quality Gate Status](https://sonarcloud.io/api/project_badges/measure?project=israellopezdeveloper_saurion&metric=alert_status)](https://sonarcloud.io/summary/new_code?id=israellopezdeveloper_saurion)
[![CI Push](https://github.com/israellopezdeveloper/saurion/actions/workflows/push.yml/badge.svg)](https://github.com/israellopezdeveloper/saurion/actions/workflows/push.yml)

![Sauron](https://raw.githubusercontent.com/israellopezdeveloper/saurion/refs/heads/metadata-branch/logo.png)

[Web Page](https://israellopezdeveloper.github.io/saurion/)

The saurion class is designed to efficiently handle asynchronous input/output
events on Linux systems using the io_uring API. Its main purpose is to
manage network operations such as socket connections, reads, writes, and
closures by leveraging an event-driven model that enhances performance and
scalability in highly concurrent applications.

The main structure, saurion, encapsulates io_uring rings and facilitates
synchronization between multiple threads through the use of mutexes and a
thread pool that distributes operations in parallel. This allows efficient
handling of I/O operations across several sockets simultaneously, without
blocking threads during operations.

The messages are composed of three main parts:

A header, which is an unsigned 64-bit number representing the length of the
message body. A body, which contains the actual message data. A footer,
which consists of 8 bits set to 0. For example, for a message with 9000
bytes of content, the header would contain the number 9000, the body would
consist of those 9000 bytes, and the footer would be 1 byte set to 0.

When these messages are sent to the kernel, they are divided into chunks using
iovec. Each chunk can hold a maximum of 8192 bytes and contains two fields:

iov_base, which is an array where the chunk of the message is stored.
iov_len, the number of bytes used in the iov_base array. For the message
with 9000 bytes, the iovec division would look like this:

The first iovec would contain: 8 bytes for the header (the length of the
message body, 9000). 8184 bytes of the message body. iov_len would be 8192
bytes in total. The second iovec would contain: The remaining 816 bytes of
the message body. 1 byte for the footer (set to 0). iov_len would be 817
bytes in total. The structure of the message is as follows:

```text
  +------------------+--------------------+----------+
  |    Header        |  Body              |  Footer  |
  |  (64 bits: 9000) |   (Message Data)   | (1 byte) |
  +------------------+--------------------+----------+
```

The structure of the iovec division is:

First iovec (8192 bytes):

```text
  +-----------------------------------------+-----------------------+
  | iov_base                                | iov_len               |
  +-----------------------------------------+-----------------------+
  | 8 bytes header, 8184 bytes of message   | 8192                  |
  +-----------------------------------------+-----------------------+
```

Second iovec (817 bytes):

```text
  +-----------------------------------------+-----------------------+
  | iov_base                                | iov_len               |
  +-----------------------------------------+-----------------------+
  | 816 bytes of message, 1 byte footer (0) | 817                   |
  +-----------------------------------------+-----------------------+
```

Each I/O event can be monitored and managed through custom callbacks that
handle connection, read, write, close, or error events on the sockets.

Basic usage example:

```c
// Create the saurion structure with 4 threads
struct saurion *s = saurion_create(4);

// Start event processing
if (saurion_start(s) != 0) {
    // Handle the error
}

// Send a message through a socket
saurion_send(s, socket_fd, "Hello, World!");

// Stop event processing
saurion_stop(s);

// Destroy the structure and free resources
saurion_destroy(s);
```

In this example, the saurion structure is created with 4 threads to handle the
workload. Event processing is started, allowing it to accept connections and
manage I/O operations on sockets. After sending a message through a socket,
the system can be stopped, and the resources are freed.
