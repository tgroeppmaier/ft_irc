#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <netinet/in.h>
#include <string>
#include <vector>
#include <queue>
#include <map>
#include "Channel.hpp"

#define MAX_CHANNELS_PER_USER 3

enum ConnectionState {
  WAITING_FOR_PASS,
  WAITING_FOR_NICK,
  WAITING_FOR_USER,
  REGISTERED
};

class Channel;

class Client {
private:
  std::map<std::string, Channel*> channels_;

public:
  int fd_;
  sockaddr_in client_addr_;
  socklen_t client_addr_len_;

  std::string nick_;
  std::string username_;
  std::string hostname_;
  // std::string servername_;
  std::string realname_;
  std::string messages_incoming_;
  std::string messages_outgoing_;
  ConnectionState state_;


  Client(int fd, sockaddr_in& client_addr, socklen_t& client_addr_len_, const std::string& hostname);
  ~Client();

  void add_buffer_to(const char* message);
  void add_message_out(const std::string message);
  void message_to_all_channels(int sender_fd, const std::string& message);
  void remove_from_all_channels();
  void add_channel(const std::string& name, Channel* channel);
  bool chan_limit_reached();

};

#endif
