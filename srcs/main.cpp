#include <iostream>
#include <string>
#include <sstream>
#include "IrcServ.hpp"
#include "MessageHandler.hpp"

using namespace std;

int main(int argc, char* argv[]) {
  if (argc < 2 || argc > 3) {
    cerr << "Usage: " << argv[0] << " <port> <password>" << endl;
    return 1;
  }

  // check for valid port range
  string port_str = argv[1];
  int port;
  stringstream ss(port_str);
  if (!(ss >> port) || !ss.eof()) {
    cerr << "Invalid port number: " << port_str << endl;
    return 1;
  }

  if (argv[2]) {
    string password = argv[2];
    IrcServ server(port, password);
    server.start();
  } 
  else {
    IrcServ server(port);
    server.start();
  }

  return 0;
}