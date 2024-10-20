#include <arpa/inet.h>
#include <unistd.h>
#include <cstring>
#include "IrcServ.hpp"


using namespace std;

void IrcServ::initializeServerAddr() {
  memset(&server_addr_, 0, sizeof(server_addr_));
  server_addr_.sin_family =
  server_addr_.sin_addr.s_addr = inet_addr("127.0.0.1"); // INADDR_ANY; Listen on all interfaces
  server_addr_.sin_port = htons(port_);
}

IrcServ::IrcServ(int port) : port_(port) {
  initializeServerAddr();
}

IrcServ::IrcServ(int port, string password) : port_(port), password_(password) {
  initializeServerAddr();
}

void IrcServ::start() {
  sock_fd_ = socket(AF_INET, SOCK_STREAM, 0);
  if (sock_fd_ == -1) {
    cerr << "Error. Failed to create socket" << strerror(errno) << endl;
    exit(EXIT_FAILURE);
  }
  
  if (bind(sock_fd_, (sockaddr*)&server_addr_, sizeof(server_addr_)) == -1) {
    cerr << "Error. Failed to bind to port " << port_ << " " << strerror(errno) << endl;
    exit(EXIT_FAILURE);
  }

  if (listen(sock_fd_, 10) == -1) {
    cerr << "Error. Failed to listen on socket " << strerror(errno) << endl;
    exit(EXIT_FAILURE);
  }
  cout << "Server started on port " << port_ << endl;

  int client1_fd;
  sockaddr_in client_addr;
  socklen_t client_addr_len = sizeof(client_addr);
  client1_fd = accept(sock_fd_, (sockaddr*)&client_addr, &client_addr_len);

  int bytes_read = 1;
  char buffer[100];
  while (bytes_read > 0) {
    bytes_read = read(client1_fd, buffer, sizeof(buffer) - 1);
    buffer[bytes_read] = '\0';
    cout << buffer << endl;
  }
}