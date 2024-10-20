# ft_irc

## useful commands

``` bash
ps auxf
lsof -p 'process number'
ss -lupn # investigate sockets
less /etc/services 
strace -p 'process num' # see system calls of process
tcpdump -i lo -X udp port 5100 
```

In the context of the provided code snippet, the "queue" refers to the backlog of pending connections that have been accepted by the operating system but not yet processed by the application. Here is a detailed explanation:

### Code Snippet
```cpp
// Start listening. Hold at most 10 connections in the queue
if (listen(sockfd, 10) < 0) {
  std::cout << "Failed to listen on socket. errno: " << errno << std::endl;
  exit(EXIT_FAILURE);
}
```

### Explanation

- **`listen(sockfd, 10)`**: This function call marks the socket referred to by `sockfd` as a passive socket, which means it will be used to accept incoming connection requests. The second parameter, `10`, specifies the maximum length of the queue for pending connections.
  
- **Queue of Pending Connections**: When a client attempts to connect to the server, the connection request is placed in a queue if the server is not immediately ready to accept it. This queue holds the pending connections until the server can process them. The length of this queue is determined by the second parameter of the `listen` function. In this case, the queue can hold up to 10 pending connections.

- **Handling the Queue**: If the queue is full (i.e., it already contains 10 pending connections), additional connection requests may be refused or ignored by the operating system until space becomes available in the queue. This ensures that the server is not overwhelmed by too many simultaneous connection requests.

- **Error Handling**: The `listen` function returns `0` on success and `-1` on failure. The condition `listen(sockfd, 10) < 0` checks if the return value is less than `0`, indicating that the `listen` operation failed. If the condition is true, the code inside the `if` block executes, printing an error message that includes the value of `errno` (which indicates the specific error) and then terminating the program using `exit(EXIT_FAILURE)`.

### Importance of the Queue

The queue is important for managing incoming connections efficiently. It allows the server to handle multiple connection requests in an orderly manner, ensuring that connections are processed as resources become available. This is particularly useful in high-traffic scenarios where multiple clients may attempt to connect to the server simultaneously.

## sockaddr vs sockaddr_in

In network programming, `sockaddr` and `sockaddr_in` are two structures used to represent socket addresses. They serve different purposes and are used in different contexts. Here is an explanation of each and their differences:

### `sockaddr` Structure

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

### `sockaddr_in` Structure

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

## accepting connection

When a client connects to a server, the connection is not handled directly by the listening socket. Instead, a new socket is created for the actual communication with the client. Here's a detailed explanation:

### Explanation

1. **Listening Socket (`sockfd`)**:
   - The listening socket is created using the `socket` function and is set up to listen for incoming connection requests using the `listen` function.
   - This socket is used solely for accepting new connections and does not handle the actual data transfer between the client and the server.

2. **Accepting a Connection**:

   ```cpp
   int addrlen = sizeof(sockaddr);
   int connection = accept(sockfd, (struct sockaddr*)&sockaddr, (socklen_t*)&addrlen);
   if (connection < 0) {
       std::cout << "Failed to grab connection. errno: " << errno << std::endl;
       exit(EXIT_FAILURE);
   }
   ```

   - The `accept` function is called on the listening socket (`sockfd`). This function extracts the first connection request on the queue of pending connections for the listening socket.
   - `accept` creates a new socket specifically for the connection with the client. This new socket is represented by the file descriptor returned by `accept`, which is stored in the `connection` variable.
   - The `sockaddr` structure is filled with the address information of the connecting client, and `addrlen` is updated to reflect the actual size of the address.

3. **New Socket for Communication**:
   - The new socket (`connection`) is used for all subsequent communication with the client. This includes reading from and writing to the socket.
   - The listening socket (`sockfd`) remains open and continues to listen for new incoming connection requests.

