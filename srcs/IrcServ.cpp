#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include <stdio.h>
#include <sys/epoll.h>
#include <pthread.h>
#include <fcntl.h>

#include "IrcServ.hpp"


using namespace std;

IrcServ::IrcServ(int port) : port_(port) {
  clients_mutex_ = PTHREAD_MUTEX_INITIALIZER;
  initializeServerAddr();
}

IrcServ::IrcServ(int port, string password) : port_(port), password_(password) {
  initializeServerAddr();
}

void set_socket(int sock_fd) {
    int flags = fcntl(sock_fd, F_GETFL, 0);
  if (flags == -1) {
    cerr << "Error. Failed to get socket flags: " << strerror(errno) << endl;
    exit(EXIT_FAILURE);
  }
  if (fcntl(sock_fd, F_SETFL, flags | O_NONBLOCK) == -1) {
    cerr << "Error. Failed to set socket to non-blocking mode: " << strerror(errno) << endl;
    exit(EXIT_FAILURE);
  }
}

void IrcServ::initializeServerAddr() {
  memset(&server_addr_, 0, sizeof(server_addr_));
  server_addr_.sin_family = AF_INET;
  server_addr_.sin_addr.s_addr = inet_addr("127.0.0.1"); // INADDR_ANY; Listen on all interfaces
  server_addr_.sin_port = htons(port_);
}


// void* IrcServ::accept_clients_wrapper(void* arg) {
//   IrcServ* server = static_cast<IrcServ*>(arg);
//   server->accept_clients();
//   return NULL;
// }

// void* IrcServ::accept_clients() {
//   while (true) {
//     epoll_event ev;
//     int n = epoll_wait(ep_fd_, &ev, 100, -1);
//     if (n == -1) {
      
//     }

//     ev.events = EPOLLIN | EPOLLET;
//     ev.data.fd = client_fd;
//     if (epoll_ctl(ep_fd_, EPOLL_CTL_ADD, client_fd, &ev) == -1) {
//       cerr << "Error adding client socket to epoll: " << strerror(errno) << endl;
//       close(client_fd);
//       continue;
//     }
    
    // sockaddr_in client_addr;
    // socklen_t client_addr_len = sizeof(client_addr);
    // int client_fd = accept(server_fd_, (sockaddr*)&client_addr, &client_addr_len);
    // if (client_fd == -1) {
    //   if (errno != EWOULDBLOCK && errno != EAGAIN) {
    //     cerr << "Error. Failed to accept connection: " << strerror(errno) << endl;
    //   }
    //   continue;
    // }

//     set_socket(client_fd);
//     // Client* client = new Client(client_fd, client_addr, client_addr_len);

//     // pthread_mutex_lock(&clients_mutex_);
//     // clients_[client_fd] = client;
//     // pthread_mutex_unlock(&clients_mutex_);
//   }
//   return NULL;
// }


void IrcServ::start() {
  server_fd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (server_fd_ == -1) {
    cerr << "Error. Failed to create socket" << strerror(errno) << endl;
    exit(EXIT_FAILURE);
  }
  set_socket(server_fd_);
  
  // create epoll instance
  ep_fd_ = epoll_create1(0);
  if (ep_fd_ == -1) {
    perror("Error. Failed to create epoll instance");
    exit(EXIT_FAILURE);
  }

  // add server socket to epoll
  epoll_event ev;
  ev.events = EPOLLIN;
  ev.data.fd = server_fd_;
  if (epoll_ctl(ep_fd_, EPOLL_CTL_ADD, server_fd_, &ev) == -1) {
    perror("Error adding server socket to epoll");
    exit(EXIT_FAILURE);
  }

  if (bind(server_fd_, (sockaddr*)&server_addr_, sizeof(server_addr_)) == -1) {
    cerr << "Error. Failed to bind to port " << port_ << " " << strerror(errno) << endl;
    exit(EXIT_FAILURE);
  }

  if (listen(server_fd_, 10) == -1) {
    cerr << "Error. Failed to listen on socket " << strerror(errno) << endl;
    exit(EXIT_FAILURE);
  }
  cout << "Server started on port " << port_ << endl;

  while (true) {
    epoll_event events[100];
    int n = epoll_wait(ep_fd_, events, 100, -1);
    if (n == -1) {
      perror("epoll_wait");
      exit(EXIT_FAILURE);
    }
    for (int i = 0; i < n; ++i) {
      if (events[i].data.fd == server_fd_) {
        cout << server_fd_ << endl;
        sockaddr_in client_addr;
        socklen_t addr_len = sizeof(client_addr);
        int client_fd = accept(server_fd_, (sockaddr*)&client_addr, &addr_len);
        if (client_fd == -1) {
          perror("Error accepting");
          if (errno == EAGAIN || errno == EWOULDBLOCK) {
            // No incoming connections, continue the loop
            continue;
          }
          else 
            exit(EXIT_FAILURE);

        }
        set_socket(client_fd);
      epoll_event client_ev;
      client_ev.events = EPOLLIN; // Use level-triggered mode (default)
      client_ev.data.fd = client_fd;
      if (epoll_ctl(ep_fd_, EPOLL_CTL_ADD, client_fd, &client_ev) == -1) {
        perror("Error adding client socket to epoll");
        close(client_fd);
        continue;
      }
      }
      else {
        char buffer[100];
        int bytes_read = read(events[i].data.fd, &buffer, sizeof(buffer) - 1);
        buffer[bytes_read] = '\0';
        cout << buffer << endl;
      }

    }
  
  }


  // int client1_fd;
  // sockaddr_in client_addr;
  // socklen_t client_addr_len = sizeof(client_addr);
  // client1_fd = accept(server_fd_, (sockaddr*)&client_addr, &client_addr_len);

  // pthread_t accept_thread;
  // int thread = pthread_create(&accept_thread, NULL, &accept_clients_wrapper, this);
  // if (thread != 0) {
  //     cerr << "Error. Failed to create accept_clients thread: " << strerror(errno) << endl;
  //     exit(EXIT_FAILURE);
  // }

  // while (true) {
  //   epoll_event events[100];
  //   int n = epoll_wait(ep_fd_, events, 100, -1);
  //   if (n == -1) {

  //   }

  //   for (int i = 0; i < n; ++i) {

  //     int bytes_read = 1;
  //     char buffer[100];
  //       bytes_read = recv(events[i].data.fd, buffer, sizeof(buffer) - 1, 0);
  //       buffer[bytes_read] = '\0';
  //       cout << buffer << endl;
  //   }

    // std::map<int, Client*>::iterator it = clients_.begin();
    // for (; it != clients_.end(); ++it) {
    //   int bytes_read = 1;
    //   char buffer[100];
    //     bytes_read = recv(it->first, buffer, sizeof(buffer) - 1, MSG_WAITALL);
    //     buffer[bytes_read] = '\0';
    //     cout << buffer << endl;
    //     // Access the key and value using it->first and it->second
    //     // std::cout << "FD: " << it->first << ", Client: " << it->second << std::endl;
    // }
  // }
}