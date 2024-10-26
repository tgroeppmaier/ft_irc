#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <netinet/in.h>
#include <string>

class Client {
private:
public:
  int fd_;
  sockaddr_in client_addr_;
  socklen_t client_addr_len_;
  std::string buffer_msg_in_;
  std::string buffer_msg_out_;

  Client(int fd, sockaddr_in client_addr, socklen_t client_addr_len_);
  ~Client();
};

#endif
