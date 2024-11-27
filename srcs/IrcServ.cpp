#include <arpa/inet.h>
#include <cstring>
#include <stdio.h>
#include <sys/epoll.h>
#include <fcntl.h>
// #include <cstdlib>
#include <signal.h>
#include <netdb.h>

#include "IrcServ.hpp"

using std::string;
using std::cout;
using std::cerr;
using std::endl;
using std::map;


IrcServ* IrcServ::instance_ = NULL;

IrcServ::IrcServ(int port) : 
  port_(port),
  server_fd_(0),
  ep_fd_(0),
  server_addr_(),
  clients_(),
  message_handler_(new MessageHandler(*this)) {
  initializeServerAddr();
  instance_ = this;
}

IrcServ::IrcServ(int port, string password) :
  port_(port), 
  server_fd_(0),
  ep_fd_(0),
  password_(password),
  server_addr_(),
  clients_(),
  message_handler_(new MessageHandler(*this)) {
  initializeServerAddr();
  instance_ = this;
}

IrcServ::~IrcServ() {
}

void IrcServ::epoll_in_out(int client_fd) {
  epoll_event ev;
  ev.events = EPOLLIN | EPOLLOUT; // Listen for both input and output events
  ev.data.fd = client_fd;
  if (epoll_ctl(ep_fd_, EPOLL_CTL_MOD, client_fd, &ev) == -1) {
    perror("Error modifying client socket to include EPOLLOUT");
    return;
  }
  // cout << "Client fd " << client_fd << " modified to in out Events" << endl;
}

void IrcServ::add_to_close(Client* client) {
  clients_to_close.insert(client);
}

void IrcServ::cleanup_clients() {
  if (clients_to_close.empty()) {
    return;
  }
  std::set<Client*>::iterator it = clients_to_close.begin();
  for (; it != clients_to_close.end(); ++it) {
    delete_client((*it)->fd_);
  }
  clients_to_close.clear();
  
}



void IrcServ::join_channel(const std::string& name, Client& client) {
  Channel* channel = channels_[name];
  channel->clients_[client.fd_] = &client;
  client.add_channel(name, channel);

  // channel->join_message_to_all(client);  

  string join_message = ":" + client.nick_ + "!" + client.username_ + "@" + client.hostname_ + " JOIN " + name + "\r\n";

  // Create and send 353 (RPL_NAMREPLY) message
  string users;
  map<int, Client*>::const_iterator it = channels_[name]->clients_.begin();
  for (; it != channels_[name]->clients_.end(); ++it) {
    users += it->second->nick_ + ' ';
    (*it).second->add_message_out(join_message);
    // it->second->messages_outgoing_.append(join_message);
    epoll_in_out(it->first);
  }
  std::string namreply_message = "353 " + client.nick_ + " = " + name + " :" + users + "\r\n";
  client.add_message_out(namreply_message);
  // client.messages_outgoing_.append(namreply_message);

  // Create and send 366 (RPL_ENDOFNAMES) message
  std::string endofnames_message = "366 " + client.nick_ + " " + name + " :End of /NAMES list\r\n";
  client.add_message_out(endofnames_message);
  // client.messages_outgoing_.append(endofnames_message);

}

Channel* IrcServ::get_channel(const std::string& name) {
  map<const string, Channel*>::const_iterator it = channels_.find(name);
  if (it == channels_.end()) {
    return NULL;
  }
  return it->second;
}


void IrcServ::create_channel(const std::string& name, Client& admin) {
  Channel* channel = new Channel(*this, name, admin);
  channels_[name] = channel;
  admin.add_channel(name, channel);

  // Create and send JOIN message
  std::string join_message = ":" + admin.nick_ + "!" + admin.username_ + "@" + admin.hostname_ + " JOIN " + name + "\r\n";
  admin.add_message_out(join_message);
  // admin.messages_outgoing_.append(join_message);

  // Create and send 353 (RPL_NAMREPLY) message
  std::string namreply_message = "353 " + admin.nick_ + " = " + name + " :" + admin.nick_ + "\r\n";
  admin.add_message_out(namreply_message);
  // admin.messages_outgoing_.append(namreply_message);

  // Create and send 366 (RPL_ENDOFNAMES) message
  std::string endofnames_message = "366 " + admin.nick_ + " " + name + " :End of /NAMES list\r\n";
  admin.add_message_out(endofnames_message);
  // admin.messages_outgoing_.append(endofnames_message);
}

void IrcServ::signal_handler(int signal) {
  cout << "Signal " << signal << " received, cleaning up and exiting..." << endl;
  if (instance_) {
      instance_->cleanup();
  }
  exit(0);
}

void IrcServ::close_socket(int fd) {
  if (fd != static_cast<uint16_t>(-1)) {
    if (close(fd) == -1) {
      perror("Error closing socket");
    } else {
      std::cout << "Socket " << fd << " closed successfully." << std::endl;
    }
  }
}

void IrcServ::delete_client(int client_fd) {
  if (epoll_ctl(ep_fd_, EPOLL_CTL_DEL, client_fd, NULL) == -1) {
    perror("Error removing client socket from epoll");
  }
  
  delete clients_[client_fd];
  clients_.erase(client_fd);
}

void IrcServ::cleanup() {
  cout << "######################KILLED BY SIGNAL#############################" << endl;
  close_socket(server_fd_);
  close_socket(ep_fd_);

  // Close and delete all client connections
  std::vector<int> client_fds;
  for (std::map<int, Client*>::iterator it = clients_.begin(); it != clients_.end(); ++it) {
    client_fds.push_back(it->first);
  }
  for (std::vector<int>::iterator it = client_fds.begin(); it != client_fds.end(); ++it) {
    delete_client(*it);
  }
  clients_.clear();

  for (std::map<string, Channel*>::iterator it = channels_.begin(); it != channels_.end(); ++it) {
    delete it->second;
  }
  channels_.clear();

  delete message_handler_;
}

void IrcServ::register_signal_handlers() {
  struct sigaction sa;
  sa.sa_handler = IrcServ::signal_handler;
  sigemptyset(&sa.sa_mask);
  sa.sa_flags = 0;

  if (sigaction(SIGINT, &sa, NULL) == -1) {
      perror("Error registering SIGINT handler");
      exit(EXIT_FAILURE);
  }

  if (sigaction(SIGTERM, &sa, NULL) == -1) {
      perror("Error registering SIGTERM handler");
      exit(EXIT_FAILURE);
  }
 
  // if (sigaction(SIGSEGV, &sa, NULL) == -1) {
  //     perror("Error registering SIGSEGV handler");
  //     exit(EXIT_FAILURE);
  // }
}

void IrcServ::set_non_block(int sock_fd) {
  int flags = fcntl(sock_fd, F_GETFL, 0);
  if (flags == -1) {
    perror("Error. Failed to get socket flags");
    exit(EXIT_FAILURE);
  }
  if (fcntl(sock_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
    perror("Error. Failed to set socket to non-blocking mode");
    exit(EXIT_FAILURE);
  }
}

void IrcServ::initializeServerAddr() {
  memset(&server_addr_, 0, sizeof(server_addr_));
  server_addr_.sin_family = AF_INET;
  server_addr_.sin_addr.s_addr = inet_addr("127.0.0.1"); //htonl(INADDR_ANY); // Listen on all interfaces
  server_addr_.sin_port = htons(static_cast<uint16_t>(port_));
}

bool IrcServ::add_fd_to_epoll(int fd) {
  epoll_event ev;
  ev.events = EPOLLIN;
  ev.data.fd = fd;
  if (epoll_ctl(ep_fd_, EPOLL_CTL_ADD, fd, &ev) == -1) {
    perror("Error adding server socket to epoll");
    return false;
    // exit(EXIT_FAILURE);
  }
  return true;
}

void IrcServ::start() {
  register_signal_handlers();
  server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd_ == -1) {
    perror("Error. Failed to create socket");
    exit(EXIT_FAILURE);
  }
  set_non_block(server_fd_);
  
  // create epoll instance
  ep_fd_ = epoll_create1(0);
  if (ep_fd_ == -1) {
    perror("Error. Failed to create epoll instance");
    exit(EXIT_FAILURE);
  }

  if(!add_fd_to_epoll(server_fd_)) {
    perror("Error adding server socket to epoll");
    exit(EXIT_FAILURE);
  }

  if (bind(server_fd_, (sockaddr*)&server_addr_, sizeof(server_addr_)) == -1) {
    perror("Error. Failed binding on port");
    cleanup();
    exit(EXIT_FAILURE);
  }

  if (listen(server_fd_, 10) == -1) {
    perror("Error. Failed to listen on socket");
    exit(EXIT_FAILURE);
  }

// Get the IP address on which the server is listening
  sockaddr_in bound_addr;
  socklen_t bound_addr_len = sizeof(bound_addr);
  if (getsockname(server_fd_, (sockaddr*)&bound_addr, &bound_addr_len) == -1) {
    perror("Error getting socket name");
    cleanup();
    exit(EXIT_FAILURE);
  }
  char* bound_ip = inet_ntoa(bound_addr.sin_addr);
  cout << "Server started on IP " << bound_ip << " and port " << port_ << endl;

  //  // Perform reverse DNS lookup to get the hostname
  // {
  //   struct hostent* he;
  //   he = gethostbyaddr((const void*)&bound_addr.sin_addr, sizeof(bound_addr.sin_addr), AF_INET);
  //   if (he == NULL) {
  //     perror("Error. Failed to resolve IP address to hostname");
  //     cleanup();
  //     exit(EXIT_FAILURE);
  //   }

  //   // Set the hostname
  //   hostname_ = he->h_name;
  // }
  // cout << "Server hostname: " << hostname_ << endl;

  event_loop();
}

void IrcServ::event_loop() {
  while (true) {
    int client_fd = -1;
    epoll_event events[100];
    int n = epoll_wait(ep_fd_, events, 100, -1);
    if (n == -1) {
      perror("epoll_wait");
      exit(EXIT_FAILURE);
    }
    for (int i = 0; i < n; ++i) {
      if (events[i].data.fd == server_fd_) { // event on server fd
        sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        client_fd = accept(server_fd_, (sockaddr*)&client_addr, &addr_len);
        if (client_fd == -1) {
          perror("Error accepting connection");
          if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // No incoming connections, continue the loop
            continue;
          } else {
            exit(EXIT_FAILURE);
          }
        }
        set_non_block(client_fd);

        // Get the client's IP address
        // char* client_ip = inet_ntoa(client_addr.sin_addr);
        // std::string client_hostname(client_ip);

        // // Optionally, attempt to get the hostname
        // struct hostent* host_entry = gethostbyaddr(&client_addr.sin_addr, sizeof(client_addr.sin_addr), AF_INET);
        // if (host_entry && host_entry->h_name) {
        //   client_hostname = host_entry->h_name;
        // }
        // cout << "Hostname: " << client_hostname << endl;

        char* client_ip = inet_ntoa(client_addr.sin_addr);
        std::string client_hostname(client_ip);


        if (!add_fd_to_epoll(client_fd)) {
          perror("Error adding client socket to epoll");
          close(client_fd);
          continue;
        }
        Client* client = new Client(client_fd, client_addr, addr_len, client_hostname); // add malloc check
        clients_[client_fd] = client;
        if (password_.empty()) {
          client->state_ = WAITING_FOR_NICK;
        }
        else {
          client->state_ = WAITING_FOR_PASS;
        }
        // cout << "Client Hostname " << client->hostname_ << endl;
        // cout << "Client State " << client->state_ << endl;
      }
      else if (events[i].events & EPOLLOUT) { // fd is ready to send messages
        client_fd = events[i].data.fd;
        Client* client = clients_[client_fd];
        message_handler_->send_messages(*client);
        cleanup_clients();
      }
      else if (events[i].events & EPOLLIN) { // event on client fd (incoming message)
        client_fd = events[i].data.fd;
        Client* client = clients_[client_fd];
        ssize_t bytes_read;
        char buffer[4096];
        if ((bytes_read = recv(client_fd, buffer, sizeof(buffer) - 1, 0)) > 0) {
          buffer[bytes_read] = '\0';
          client->add_buffer_to(buffer);
          // cout << "\033[31m" << buffer << "\033[0m" << endl; // debug purposes       
          message_handler_->process_incoming_messages(*client);
        } 
        else if (bytes_read == 0 || (bytes_read == -1 && errno != EWOULDBLOCK && errno != EAGAIN)) {
          // Handle client disconnection or error
          if (bytes_read == 0) {
              cout << "Client disconnected" << endl;
          } else {
              perror("Error. Failed to read from client");
          }
          delete_client(client_fd);
        }
      }
    }
  }
}
