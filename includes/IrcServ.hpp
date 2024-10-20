#ifndef IRCSERV_HPP
#define IRCSERV_HPP

#include <netinet/in.h>
#include <sys/socket.h>
#include <iostream>
#include <string>
#include <cerrno>
#include <cstdlib>

class IrcServ {
private:
  int port_;
  int sock_fd_;
  std::string password_;
  sockaddr_in server_addr_;

  void initializeServerAddr();

public:
  IrcServ(int port);
  IrcServ(int port, std::string password);
  // IrcServ(const IrcServ &other);
  // IrcServ &operator=(const IrcServ &other);
  // ~IrcServ();

  void start();

  // int SetPort(int port);
};

#endif
