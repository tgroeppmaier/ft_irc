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

void Client::message_to_all_channels(const std::string& message) {
  for (std::map<std::string, Channel*>::iterator it = channels_.begin(); it != channels_.end(); ++it) {
    it->second->broadcast(message);
  }
}

void Client::remove_from_all_channels() {
  for (map<string, Channel*>::iterator it = channels_.begin(); it != channels_.end(); ++it) {
    it->second->remove_client(*this);
  }
  channels_.clear();
}

void Client::broadcast_all_channels(const std::string& message) {
  for (std::map<std::string, Channel*>::iterator it = channels_.begin(); it != channels_.end(); ++it) {
    it->second->broadcast(message);
  }
}

void Client::add_channel(const string& name, Channel* channel) {
  channels_[name] = channel;
}

void Client::remove_channel(const string& name) {
  channels_.erase(name);
}

bool Client::chan_limit_reached() {
  if (channels_.size() >= MAX_CHANNELS_PER_USER) {
    return true;
  }
  return false;
}

