#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include "Client.hpp"



// void Client::initializeServerAddr() {
//   memset(&server_addr_, 0, sizeof(server_addr_));
//   server_addr_.sin_family =
//   server_addr_.sin_addr.s_addr = inet_addr("127.0.0.1"); // INADDR_ANY; Listen on all interfaces
//   server_addr_.sin_port = htons(port_);
// }


Client::Client(int fd, sockaddr_in& client_addr, socklen_t& client_addr_len)
    : fd_(fd), client_addr_(client_addr), client_addr_len_(client_addr_len) {
}

Client::~Client() {
  close(fd_);
}

void Client::add_buffer_to(const char* message) {
  if (message) {
    buffer_msg_from_.append(message);
  }
}

void Client::split_buffer() {
  size_t start = 0;
  size_t end;
  // vector<string> messages;
  while ((end = buffer_msg_from_.find("\r\n", start)) != string::npos) {
    messages_.push_back(buffer_msg_from_.substr(start, end - start));
    start = end + 2;
  }
  if (start > buffer_msg_from_.size()) {
    start -= 2;
  }  
  buffer_msg_from_.erase(0, start);;
}

