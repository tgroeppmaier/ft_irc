#include "MessageHandler.hpp"
#include <iostream>

namespace MessageHandler {

void process_messages(Client& client) {
    client.split_buffer();
}

} // namespace MessageHandler