#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <netinet/in.h>
#include <string>
#include <vector>
#include <queue>
#include <map>
#include "Channel.hpp"


class Channel;

class Client {
private:

public:
  int fd_;
  sockaddr_in client_addr_;
  socklen_t client_addr_len_;

  std::string nick_;
  std::string username_;
  std::string hostname_;
  std::string servername_;
  std::string realname_;
  std::string messages_incoming_;
  std::string messages_outgoing_;

  std::map<std::string, Channel*> channels_;

  Client(int fd, sockaddr_in& client_addr, socklen_t& client_addr_len_);
  ~Client();

  void add_buffer_to(const char* message);
  void add_message_out(const std::string message);

};

#endif
