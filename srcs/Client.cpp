#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include "Client.hpp"

using std::string;
using std::vector;

Client::Client(int fd, sockaddr_in& client_addr, socklen_t& client_addr_len, const std::string& hostname)
  : fd_(fd),
    client_addr_(client_addr),
    client_addr_len_(client_addr_len),
    nick_(""),
    username_(""),
    hostname_(hostname),
    // servername_(""),
    realname_(""),
    messages_incoming_("")
    {
}

Client::~Client() {
  if (fd_ > 0) {
    close(fd_);
  }
}

void Client::add_buffer_to(const char* message) {
  if (message) {
    messages_incoming_.append(message);
  }
}

void Client::add_message_out(const std::string message) {
  messages_outgoing_.append(message);
}