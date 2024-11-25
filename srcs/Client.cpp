#include <netinet/in.h>
#include <unistd.h>
#include <cstring>
#include "Client.hpp"

using std::string;
using std::vector;
using std::map;

Client::Client(int fd, sockaddr_in& client_addr, socklen_t& client_addr_len, const std::string& hostname)
  : fd_(fd), client_addr_(client_addr), client_addr_len_(client_addr_len), hostname_(hostname) {
    messages_incoming_.reserve(256);
    messages_outgoing_.reserve(256);
}

Client::~Client() {
  if (fd_ > 0) {
    close(fd_);
  }
  remove_from_all_channels();
}

void Client::add_buffer_to(const char* message) {
  if (message) {
    messages_incoming_.append(message);
  }
}

void Client::add_message_out(const std::string message) {
  messages_outgoing_.append(message);
}

void Client::message_to_all_channels(int sender_fd, const std::string& message) {
  map<string, Channel*>::const_iterator it;
  for (it = channels_.begin(); it != channels_.end(); ++it) {
    (*it).second->broadcast(sender_fd, message);
  }
}

void Client::remove_from_all_channels() {
  map<string, Channel*>::iterator it;
  for (it = channels_.begin(); it != channels_.end(); ++it) {
    (*it).second->remove_client(*this);
  }
}

void Client::add_channel(const string& name, Channel* channel) {
  channels_[name] = channel;
}

