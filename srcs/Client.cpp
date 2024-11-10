#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include "Client.hpp"

using std::string;
using std::vector;

Client::Client(int fd, sockaddr_in& client_addr, socklen_t& client_addr_len)
  : fd_(fd),
    client_addr_(client_addr),
    client_addr_len_(client_addr_len),
    nick_(""),
    username_(""),
    hostname_(""),
    servername_(""),
    realname_(""),
    buffer_in("")
    // received_messages_() 
    {
}

Client::~Client() {
  if (fd_ > 0) {
    close(fd_);
  }
}

void Client::add_buffer_to(const char* message) {
  if (message) {
    buffer_in.append(message);
  }
}

// void Client::split_buffer() {
//   if (buffer_in.empty()) {
//     return;
//   }
//   size_t start = 0;
//   size_t end;
//   while ((end = buffer_in.find("\r\n", start)) != std::string::npos) {
//     received_messages_.push_back(buffer_in.substr(start, end - start));
//     start = end + 2;
//   }
//   if (start > buffer_in.size()) {
//     start -= 2;
//   }
//   buffer_in.erase(0, start);
// }