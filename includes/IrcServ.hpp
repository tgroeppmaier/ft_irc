#ifndef IRCSERV_HPP
#define IRCSERV_HPP

#include <map>
#include <set>
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
  MessageHandler* message_handler_;
  std::map<std::string, Channel*> channels_;
  std::set<Client*> clients_to_close; 
  std::map<int, Client*> clients_;    
  std::string password_;
  std::string hostname_;
  int port_;                           
  int server_fd_;                      
  int ep_fd_;                          

  // Private constructors
  IrcServ(int port, const std::string& password);
  IrcServ(const IrcServ&);  // Prevent copying
  IrcServ& operator=(const IrcServ&);  // Prevent assignment

  static void signal_handler(int signal);
  void set_non_block(int sock_fd);
  void cleanup_clients();
  void initializeServerAddr();
  bool add_fd_to_epoll(int fd);
  void register_signal_handlers();
  void event_loop();

public:
  static IrcServ& getInstance(int port = 0, const std::string& password = "");

  sockaddr_in server_addr_;

  ~IrcServ();
  void start();
  void cleanup();
  void close_socket(int fd);
  std::string to_upper(const std::string& str);
  void delete_client(int fd);
  void add_to_close(Client* client);
  void create_channel(const std::string& name, Client& admin);
  void delete_channel(const std::string& name);
  void epoll_in_out(int fd);
  void epoll_in(int fd);
  bool check_password(const std::string& password);
  bool check_nick(const std::string& nick);
  Channel* get_channel(const std::string& name);
  Client* get_client(const std::string& name);
};

#endif
