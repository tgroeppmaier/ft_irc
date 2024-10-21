#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include "Client.hpp"


using namespace std;

// void Client::initializeServerAddr() {
//   memset(&server_addr_, 0, sizeof(server_addr_));
//   server_addr_.sin_family =
//   server_addr_.sin_addr.s_addr = inet_addr("127.0.0.1"); // INADDR_ANY; Listen on all interfaces
//   server_addr_.sin_port = htons(port_);
// }


Client::Client(int fd, sockaddr_in client_addr, socklen_t client_addr_len)
    : fd_(fd), client_addr_(client_addr), client_addr_len_(client_addr_len) {
}

Client::~Client() {
  close(fd_);
}

