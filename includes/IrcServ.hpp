#ifndef IRCSERV_HPP
#define IRCSERV_HPP

#include <map>
#include <netinet/in.h>
#include <sys/socket.h>
#include <iostream>
#include <sys/epoll.h>
#include <string>
#include <cerrno>
#include <cstdlib>
#include "Client.hpp"


class IrcServ {
private:
  int port_;
  int server_fd_;
  std::string password_;
  sockaddr_in server_addr_;
  pthread_mutex_t clients_mutex_; 

  int ep_fd_;
  // struct epoll_event ev_;


  void initializeServerAddr();
  // static void* accept_clients_wrapper(void* arg);
  // void* accept_clients();

public:
  std::map<int, Client*> clients_; 

  IrcServ(int port);
  IrcServ(int port, std::string password);
  // IrcServ(const IrcServ &other);
  // IrcServ &operator=(const IrcServ &other);
  // ~IrcServ();

  void start();

  // int SetPort(int port);
};

#endif
