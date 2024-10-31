#include "MessageHandler.hpp"
#include <iostream>
#include <sstream>

MessageHandler::MessageHandler(IrcServ& server) : server_(server) {
    // Constructor implementation (if needed)
}

void MessageHandler::split_buffer(Client& client) {
  if (client.buffer_msg_from_.empty()) {
      return;
  }
  size_t start = 0;
  size_t end;
  while ((end = client.buffer_msg_from_.find("\r\n", start)) != std::string::npos) {
      client.messages_.push_back(client.buffer_msg_from_.substr(start, end - start));
      start = end + 2;
  }
  if (start > client.buffer_msg_from_.size()) {
      start -= 2;
  }
  client.buffer_msg_from_.erase(0, start);
}

// string MessageHandler::get_command(std::string& message) {
//     std::string command;
//     std::stringstream ss(message);
//     if (ss >> command) {
//         std::map<std::string, void(*)(Client&)>::iterator cmd_it = server_.command_map_.find(command);
//         if (cmd_it != server_.command_map_.end()) {
//             return command;
//         } 
//         else {
//             return
//         }
//     }
// }

void MessageHandler::process_incoming_messages(Client& client) {
  split_buffer(client);
  for (vector<string>::iterator it = client.messages_.begin(); it != client.messages_.end(); ++it) {
    stringstream ss(*it);
    string command;
    vector<string> arguments;
    if (ss >> command) {
      string arg;
      while (ss >> arg) {
        arguments.push_back(arg);
      }
    }
    else {
      continue;
    }
    map<string, void(*)(Client&, vector<string>&)>::iterator cmd_it = server_.command_map_.find(command);
    if (cmd_it != server_.command_map_.end()) {
        cmd_it->second(client, arguments);
    } else {
        std::cout << "Unknown command: " << command << std::endl;
    }
  }
    client.messages_.clear(); // Clear messages after processing
}