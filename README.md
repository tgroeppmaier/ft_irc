# ft_irc Internet Relay Chat Server

- [ft\_irc Internet Relay Chat Server](#ft_irc-internet-relay-chat-server)
  - [Project Instructions](#project-instructions)
  - [Starting the Server](#starting-the-server)
    - [Testing](#testing)
      - [Useful Shell Commands](#useful-shell-commands)
  - [Reference Client](#reference-client)
    - [Weechat Commands](#weechat-commands)
  - [Useful Links](#useful-links)
  - [Technicals](#technicals)
    - [Design choices](#design-choices)
    - [High level overview](#high-level-overview)
    - [Stream Sockets](#stream-sockets)
      - [How Stream Sockets Work](#how-stream-sockets-work)
      - [Use Cases](#use-cases)
      - [Socket System Calls](#socket-system-calls)
      - [socket()](#socket)
      - [bind()](#bind)
      - [listen()](#listen)
      - [accept()](#accept)
      - [connect()](#connect)
      - [send()](#send)
      - [recv()](#recv)
      - [close()](#close)
      - [sockaddr vs sockaddr\_in](#sockaddr-vs-sockaddr_in)
        - [`sockaddr` Structure](#sockaddr-structure)
        - [`sockaddr_in` Structure](#sockaddr_in-structure)
    - [Differences and Relationship](#differences-and-relationship)
    - [epoll](#epoll)
      - [Key Aspects of `epoll`](#key-aspects-of-epoll)

## Project Instructions

Develop an IRC server in c++ 98 using non-blocking IO operations.

- No Server to Server communication
- You must be able to:
  - Authenticate
  - Set a nickname
  - Set a username
  - Join a channel
  - Send and receive private messages using your reference client
  - Forward all messages sent from one client to a channel to every other client that joined the channel
- You must have operators and regular users
- Implement the commands that are specific to channel operators:
  - **KICK**: Eject a client from the channel
  - **INVITE**: Invite a client to a channel
  - **TOPIC**: Change or view the channel topic
  - **MODE**: Change the channel`s mode:
    - `i`: Set/remove Invite-only channel
    - `t`: Set/remove the restrictions of the TOPIC command to channel operators
    - `k`: Set/remove the channel key (password)
    - `o`: Give/take channel operator privilege
    - `l`: Set/remove the user limit to channel

## Starting the Server

This server uses **epoll** because it offers better performance than **select** and **poll**.  
However, since **epoll** is a Linux system call, this server runs only on Linux.

- **Download**: `git clone git@github.com:tgroeppmaier/ft_irc.git`
- **Build**: `make`
- **Make file executable**: `chmod +x ircserv`
- **Run**: `./ircserv <port> <optional password>`

### Testing

The server can be tested with `nc <hostname / ip> <port>`. The server handles partially sent messages but each full message must manually be terminated with Ctrl + V, Ctrl + M to insert a carriage return (\r) and then enter (\n). The server can also be connected to with telnet `telnet <hostname/ip> <port>` and then all the IRC commands work the same as from a client programm. Telnet is also used in `test.sh`, which spams a channel with thousands of messages per second (after deactivating anti flood in the client).

#### Useful Shell Commands

- `netstat -tulnp`: Investigate sockets.
- `ss -tulnp`: Investigate sockets using `ss`.
- `lsof -i`: List open files (use `-i` for TCP/UDP).
- `ps auxf`: Display detailed process information.
- `lsof -p <process_number>`: List open files for a specific process.
- `less /etc/services`: View the services file.
- `strace -p <process_number>`: Trace system calls of a specific process.
- `tcpdump -i lo -X tcp port 6667`: Capture and display packets on the loopback interface for TCP port 6667.

## Reference Client

**Weechat** is the reference client that was used to test the server with.

### Weechat Commands

- **Start**: `weechat`
- **Start with different home folder**: `weechat --dir /path/to/folder`
- **Add server**: `/server add <server_name> <server_address>/<port>`
  - Example: `/server add ftirc 127.0.0.1/6667`
- **See all options**: `/set`
- **Set server options**:
  - Disable TLS: `/set irc.server.<server_name>.tls off`
  - Set username: `/set irc.server.<server_name>.username "My user name"`
  - Set real name: `/set irc.server.<server_name>.realname "My real name"`
  - Set nicknames: `/set irc.server_default.nicks "name"`
  - Disable anti-flood: `/set irc.server.<server_name>.anti_flood_prio_low 0`
  - Disable default anti-flood: `/set irc.server_default.anti_flood 0`
- **Send private message**: `/msg <recipient nickname> message`
- **Show raw IRC command**: `/server raw`
- **Switch channel / Buffer**: `F5`
- **List servers**: `/server list`

## Useful Links

- [IRC Protocol Documentation](https://ircgod.com/docs/irc/to_know/)
- [ft_irc Wiki](https://github.com/42-serv/ft_irc/wiki)
- [Introduction to Linux epoll](https://expserver.github.io/guides/resources/introduction-to-linux-epoll.html)
- [What is epoll?](https://medium.com/@avocadi/what-is-epoll-9bbc74272f7c)
- [Modern IRC docs](https://modern.ircdocs.horse)

## Technicals

### Design choices

This server uses the more modern RFC 2812 IRC implementation.  
Because of the requirement to check the sockets readiness before every send and receive, all the message to a client will be sent at once in one long cstring. This is possible because IRC messages all end with `\r\n`. For the receiving a large buffer of 4096 bytes is used to read many or most likely all messages that come from one client

### High level overview

Server and client application are different processes running on some machine. For security reasons has each proccess its own memory space and cannot directly interact with another process. But to have a functional server, it needs to be able to exchange data with the client.  
To overcome the isolation a process needs to request a form of **IPC** inter process communication from the operating system through **system calls**.  
There are different forms of **IPC** of which depending on the context one form might be better suited than another. One that most shell users are already familiar with is the **pipe** which allows chaining output of one process into the input for the next process. A **pipe** though can only be used by related processes (**fork**) running on the same machine. Since a the IRC server usually is not running on the same machine as the clients, a **Socket** and more specifically a **Internet Domain Socket** is the natural choice.

### Stream Sockets

Stream sockets, often referred to as **SOCK_STREAM**, are a type of socket that provide **reliable, two-way, connection-oriented communication streams**. These sockets are a fundamental part of network programming and are often used when data integrity and order are crucial. They provide a reliable and ordered way to transmit data between applications, making them suitable for many network applications that require guaranteed data delivery and integrity.

- **Connection-Oriented:** Stream sockets establish a connection between two endpoints before data transfer begins. This is similar to making a phone call, where a connection is made before any conversation can take place.
- **Reliable:** Data sent via stream sockets is guaranteed to arrive in the order it was sent, and without errors. The underlying protocol (typically TCP) handles error detection and retransmission, ensuring data integrity.
- **Two-way Communication:** Once a connection is established, data can flow in both directions. This makes stream sockets suitable for applications where both the client and the server need to send data back and forth.
- **Byte Stream:** Stream sockets treat data as a continuous stream of bytes without message boundaries. The receiving end can read data in chunks of arbitrary sizes, regardless of how it was written at the sending end.
- **TCP Protocol:** Stream sockets commonly use the Transmission Control Protocol (TCP). TCP provides the reliability and connection-oriented features of stream sockets. TCP ensures data arrives sequentially and error-free.
- **File Descriptor:** Stream sockets use standard Unix file descriptors, which means they can be treated like files. This allows programs to use standard I/O operations like `read()` and `write()` (although `send()` and `recv()` are typically preferred for greater control).

#### How Stream Sockets Work

- **Creation:** A stream socket is created using the `socket()` system call, specifying the `SOCK_STREAM` type.
- **Binding:** On the server side, the socket is bound to a specific address (IP address and port number) using the `bind()` system call. This makes the server listen on a specific port.
- **Listening:** The `listen()` system call puts the server socket into a listening state, enabling it to accept incoming connection requests.
- **Connection:** A client establishes a connection to the server by calling the `connect()` system call, specifying the server's address.
- **Accepting Connections:** When a client connects, the server uses the `accept()` system call to create a new socket descriptor for that specific connection.
- **Data Transfer:** Once the connection is established, data is sent and received using the `send()` and `recv()` system calls.

#### Use Cases

- **Web Browsing (HTTP):** Web browsers use HTTP over stream sockets to fetch web pages.
- **Secure Shell (SSH):** SSH uses stream sockets to provide a secure, reliable connection.
- **Telnet:** The telnet protocol uses stream sockets for remote terminal access.
- **File Transfer (FTP):** FTP uses stream sockets for reliable transfer of files.
- **Email (SMTP):** SMTP uses stream sockets to send and receive email messages.

#### Socket System Calls

- `socket()`: Creates a new socket.
- `bind()`: Associates a socket with a local address.
- `listen()`: Enables a socket to accept incoming connections.
- `accept()`: Accepts an incoming connection, creating a new socket.
- `connect()`: Establishes a connection to a remote host.
- `send()`: Sends data over a socket.
- `recv()`: Receives data from a socket.
- `close()`: Closes a socket connection.

#### socket()

Creates a new socket and returns a file descriptor.

```c
int sockfd = socket(PF_INET, SOCK_STREAM, 0);
if (sockfd == -1) {
    perror("socket");
    exit(EXIT_FAILURE);
}
```

- `PF_INET`: IPv4 protocol family
- `SOCK_STREAM`: TCP socket type
- Returns -1 on error

#### bind()

Associates a socket with a specific address and port.

```c
struct sockaddr_in addr;
memset(&addr, 0, sizeof(addr));
addr.sin_family = AF_INET;
addr.sin_port = htons(8080);
addr.sin_addr.s_addr = INADDR_ANY;

if (bind(sockfd, (struct sockaddr*)&addr, sizeof(addr)) == -1) {
    perror("bind");
    exit(EXIT_FAILURE);
}
```

- Used primarily by servers
- `INADDR_ANY` allows binding to all network interfaces
- Port number must be converted to network byte order with `htons()`

#### listen()

Marks a socket as passive, ready to accept incoming connections.

```c
if (listen(sockfd, BACKLOG) == -1) {
    perror("listen");
    exit(EXIT_FAILURE);
}
```

- `BACKLOG` specifies maximum pending connections queue length
- Only used for stream sockets (TCP)
  
#### accept()

Accepts a connection on a listening socket.

```c
struct sockaddr_in client_addr;
socklen_t addr_size = sizeof(client_addr);
int client_fd = accept(sockfd, (struct sockaddr*)&client_addr, &addr_size);
if (client_fd == -1) {
    perror("accept");
    exit(EXIT_FAILURE);
}
```

- The listening socket (`sockfd`) remains open and continues to listen for new incoming connection requests.

- Blocks until a connection arrives
- Extracts the first connection request on the queue of pending connections for the listening socket.
- Returns a new socket, that is used for all subsequent communication with the client. This includes reading from and writing to the socket.
- Original socket remains in listening state
- The `sockaddr` structure is filled with the address information of the connecting client, and `addrlen` is updated to reflect the actual size of the address.

#### connect()

Initiates a connection to a remote server.

```c
struct sockaddr_in server_addr;
memset(&server_addr, 0, sizeof(server_addr));
server_addr.sin_family = AF_INET;
server_addr.sin_port = htons(8080);
inet_pton(AF_INET, "127.0.0.1", &server_addr.sin_addr);

if (connect(sockfd, (struct sockaddr*)&server_addr, sizeof(server_addr)) == -1) {
    perror("connect");
    exit(EXIT_FAILURE);
}
```

- Used by clients to connect to servers
- Blocks until connection is established
- Returns -1 on error

#### send()

Sends data over a connected socket.

```c
char *message = "Hello, server!";
ssize_t bytes_sent = send(sockfd, message, strlen(message), 0);
if (bytes_sent == -1) {
    perror("send");
    exit(EXIT_FAILURE);
}
```

- Returns number of bytes sent
- May send fewer bytes than requested
- Last parameter is for flags (0 for normal operation)

#### recv()

Receives data from a connected socket.

```c
char buffer[1024];
ssize_t bytes_received = recv(sockfd, buffer, sizeof(buffer), 0);
if (bytes_received == -1) {
    perror("recv");
    exit(EXIT_FAILURE);
}
buffer[bytes_received] = '\0';  // Null-terminate received data
```

- Returns number of bytes received
- Returns 0 when peer has performed orderly shutdown
- May receive fewer bytes than buffer size

#### close()

Closes a socket and releases resources.

```c
if (close(sockfd) == -1) {
    perror("close");
    exit(EXIT_FAILURE);
}
```

- Terminates both sending and receiving on the socket
- Releases system resources
- Should be called when socket is no longer needed

Each of these examples includes proper error checking and demonstrates the essential usage of the system call. Remember to include necessary headers and handle errors appropriately in production code.

#### sockaddr vs sockaddr_in

In network programming, `sockaddr` and `sockaddr_in` are two structures used to represent socket addresses. They serve different purposes and are used in different contexts. Here is an explanation of each and their differences:

##### `sockaddr` Structure

- **Definition**: `sockaddr` is a generic structure used to represent a socket address. It is defined in the `<sys/socket.h>` header file.
- **Purpose**: It serves as a base type for various specific socket address structures, such as `sockaddr_in` for IPv4 and `sockaddr_in6` for IPv6.
- **Fields**:

  ```c
  struct sockaddr {
      sa_family_t sa_family; // Address family (e.g., AF_INET for IPv4)
      char sa_data[14];      // Address data (protocol-specific)
  };
  ```

- **Usage**: Functions like `bind`, `connect`, and `accept` use `sockaddr` as a parameter to accept different types of socket addresses. You typically cast specific address structures (like `sockaddr_in`) to `sockaddr` when calling these functions.

##### `sockaddr_in` Structure

- **Definition**: `sockaddr_in` is a specific structure used to represent an IPv4 socket address. It is defined in the `<netinet/in.h>` header file.
- **Purpose**: It provides a more detailed and specific representation of an IPv4 address, including the IP address and port number.
- **Fields**:

  ```c
  struct sockaddr_in {
      sa_family_t sin_family;   // Address family (AF_INET for IPv4)
      in_port_t sin_port;       // Port number (in network byte order)
      struct in_addr sin_addr;  // IPv4 address (in network byte order)
      unsigned char sin_zero[8]; // Padding to make the structure the same size as sockaddr
  };
  ```

- **Usage**: You use `sockaddr_in` to specify IPv4 addresses in your code. When calling functions like `bind`, `connect`, or `accept`, you cast `sockaddr_in` to `sockaddr`.

### Differences and Relationship

- **Generic vs. Specific**: `sockaddr` is a generic structure that can represent any type of socket address, while `sockaddr_in` is specific to IPv4 addresses.
- **Fields**: `sockaddr` has a generic `sa_data` field to hold address data, whereas `sockaddr_in` has specific fields for the address family, port number, and IP address.
- **Typecasting**: When using functions like `bind`, `connect`, or `accept`, you need to cast `sockaddr_in` to `sockaddr` because these functions expect a pointer to `sockaddr`.

### epoll

`epoll` is a Linux-specific API for I/O event notification, designed to improve upon the capabilities of `select()` and `poll()`. It provides a mechanism for monitoring multiple file descriptors to see if I/O is possible on any of them. The `epoll` API offers better performance, especially when dealing with a large number of open connections.

#### Key Aspects of `epoll`

- **Core Concepts:** `epoll` is based around three system calls:
  - `epoll_create()`: Creates an `epoll` instance, which is an object maintained by the kernel that is used to monitor file descriptors. It returns a file descriptor associated with the `epoll` instance.
  - `epoll_ctl()`: Modifies the **interest list** of an `epoll` instance, adding, modifying, or removing file descriptors from the list of those being monitored. You can specify what events to monitor for each file descriptor.
  - `epoll_wait()`: Waits for I/O events to occur on any of the file descriptors in the interest list. It returns a list of file descriptors that have pending I/O events.

- **How it Works:**
  1. An `epoll` instance is created using `epoll_create()`.
  2. File descriptors of interest (e.g., sockets, pipes) are added to the `epoll` instance's interest list using `epoll_ctl()`. For each file descriptor, the program specifies which events to monitor (e.g., read, write, error).
  3. The application then calls `epoll_wait()`, which blocks until an event occurs on one of the monitored file descriptors.
  4. `epoll_wait()` returns the list of file descriptors that have events waiting to be processed.
  5. The application handles the events and goes back to step 3, continuing to use `epoll_wait()` to monitor for new events.

- **Edge-Triggered vs. Level-Triggered Notification:**
  - **Level-triggered notification** is used by `select()` and `poll()` and means that if a file descriptor is ready for I/O, `select()` and `poll()` will continue to report it as ready until the application reads all of the data. In other words, they report the level of readiness.
  - **Edge-triggered notification**, supported by `epoll`, will only report a change of state. This means that if an application does not read all of the data after the file descriptor is flagged as readable, then `epoll` will not report that file descriptor as readable again until more data is received. Edge-triggered notification is more efficient but also more complex for the programmer to use.

- **Advantages of `epoll` Over `select()` and `poll()`:**
  - **Performance:** `epoll` performs better than `select()` and `poll()`, especially when monitoring a large number of file descriptors. `epoll` uses an event table within the kernel that is more efficient to manage than the file descriptor sets used by `select()` and `poll()`.
  - **Scalability:** `epoll` scales better to large numbers of file descriptors. It is more suited to servers handling thousands of concurrent connections.
  - **Efficiency:** `epoll` uses a more efficient mechanism in the kernel. It only returns file descriptors that have pending I/O events, while `select()` and `poll()` may return file descriptors that are not ready for I/O.

- **Use Cases:**
  - **High-Performance Network Servers:** `epoll` is well-suited for building scalable network servers that need to handle a large number of concurrent connections efficiently.
  - **I/O Intensive Applications:** Applications that need to monitor multiple file descriptors for read and write events can benefit from `epoll`'s efficient event notification mechanism.
  - **Any application needing high performance and scalability when dealing with multiple file descriptors.**

- **Integration with other APIs:** `epoll` can be used to monitor file descriptors created by other APIs, such as `signalfd()`, which provides a way to monitor signals using file descriptors, and `timerfd()`, which provides a way to monitor timers using file descriptors. It can also be used with eventfd objects and inotify file descriptors.

In summary, `epoll` is a powerful API for I/O event notification on Linux, providing significant performance and scalability improvements compared to `select()` and `poll()`. It's most useful for high-performance and high-concurrency applications.



