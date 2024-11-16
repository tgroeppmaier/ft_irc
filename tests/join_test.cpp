#include "MessageHandler.hpp"
#include <iostream>
#include <sstream>
#include <cstdio>
#include <sys/epoll.h>
#include <string>
#include <arpa/inet.h> // For inet_ntoa


using namespace std;


void split_message(string message) {
  vector<string> args;
  string tmp;
  stringstream ss(message);
  while (ss >> tmp) {
    if (!tmp.empty()) {
      args.push_back(tmp);
    }
  }
  command_JOIN(args);
}

void command_JOIN(std::vector<std::string>& arg) {
  vector<string> tokens;

  for (vector<string>::const_iterator it = arg.begin(); it != arg.end(); ++it) {
    string token;
    std::stringstream ss(*it);
    while(std::getline(ss, token, ',')) {
      if (!token.empty()) {
        tokens.push_back(token);
      }
    }
  }
  vector<string> channels;
  vector<string> keys;
  for (vector<string>::const_iterator it = tokens.begin(); it != tokens.end(); ++it) {
    string tmp = *it;
    if (!tmp.empty() && (tmp[0] == '#' || tmp[0] == '&')) {
      channels.push_back(tmp);
    }
    else {
      keys.push_back(tmp);
    }
  }
}

int main() {
  

}