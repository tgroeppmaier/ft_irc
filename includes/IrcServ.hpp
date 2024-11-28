#ifndef IRCSERV_HPP
#define IRCSERV_HPP

#include <map>
#include <set>
#include <queue>
#include <netinet/in.h>
#include <sys/socket.h>
#include <iostream>
#include <sys/epoll.h>
#include <string>
#include <cerrno>
#include <cstdlib>
#include "MessageHandler.hpp"
#include "Client.hpp"
#include "Channel.hpp"

class MessageHandler;
class Channel;
class Client;

class IrcServ {
private:
  int port_;
  int server_fd_;
  static IrcServ* instance_;
  

  static void signal_handler(int signal);
  void set_non_block(int sock_fd);
  void cleanup_clients();

  void initializeServerAddr();
  bool add_fd_to_epoll(int fd);
  void register_signal_handlers();
  void event_loop();
  std::set<Client*> clients_to_close;


public:
  int ep_fd_;
  std::string password_;
  std::string hostname_;
  sockaddr_in server_addr_;
  std::map<int, Client*> clients_;
  std::map<std::string, Channel*> channels_;
  MessageHandler* message_handler_;

  IrcServ(int port);
  IrcServ(int port, std::string password);
  ~IrcServ();
  void close_socket(int fd);
  void delete_client(int fd);
  void add_to_close(Client* client);

  void create_channel(const std::string& name, Client& admin);
  void join_channel(const std::string& name, Client& admin);

  void epoll_in_out(int fd);

  Channel* get_channel(const std::string& name);
  // Client* get_client(const std::string& name);

  // IrcServ(const IrcServ &other);
  // IrcServ &operator=(const IrcServ &other);

  void start();
  void cleanup();

  // int SetPort(int port);
};

#endif
