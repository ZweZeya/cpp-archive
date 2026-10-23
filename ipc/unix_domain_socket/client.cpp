#include "sys/socket.h"
#include "sys/un.h"
#include "unistd.h"
#include <iostream>
#include <cstring>
#include <string>

const char* SOCKET_PATH = "/tmp/mysocket_example.sock";

int main() {
    int client_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (client_fd < 0) {
        std::cerr << "Socket failed\n";
        return 1;
    }

    struct sockaddr_un server_addr{};
    server_addr.sun_family = AF_UNIX;
    strncpy(server_addr.sun_path, SOCKET_PATH, sizeof(server_addr.sun_path) - 1);

    if (connect(client_fd, reinterpret_cast<struct sockaddr*>(&server_addr), sizeof(server_addr)) == -1) {
        std::cerr << "Connect failed\n";
        close(client_fd);
        return 1;
    }

    std::string message = "Hello world";

    if (write(client_fd, message.c_str(), message.length()) == -1) {
        std::cerr << "Write failed\n";
        close(client_fd);
        return 1;
    }

    close(client_fd);
    return 0;
}