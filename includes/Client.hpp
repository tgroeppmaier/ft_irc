#ifndef CLIENT_HPP
#define CLIENT_HPP

#include <netinet/in.h>
#include <string>
#include <vector>

using namespace std;


class Client {
private:

public:
  int fd_;
  sockaddr_in client_addr_;
  socklen_t client_addr_len_;
  std::string nick_;
  std::string buffer_msg_to_;
  std::string buffer_msg_from_;

  Client(int fd, sockaddr_in& client_addr, socklen_t& client_addr_len_);
  ~Client();

  std::vector<std::string> split_messages();
  void add_buffer_to(const char* message);

};

#endif
