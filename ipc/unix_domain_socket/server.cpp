#include "sys/socket.h"
#include "sys/un.h"
#include "unistd.h"
#include <iostream>
#include <cstring>

const char* SOCKET_PATH = "/tmp/mysocket_example.sock";

int main() {
    int server_fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (server_fd < 0) {
        std::cerr << "Socket failed\n";
        return 1;
    }

    unlink(SOCKET_PATH);

    struct sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strncpy(addr.sun_path, SOCKET_PATH, sizeof(addr.sun_path) - 1);

    if (bind(server_fd, reinterpret_cast<struct sockaddr*>(&addr), sizeof(addr)) == -1) {
        std::cerr << "Bind failed\n";
        close(server_fd);
        return 1;
    }

    if (listen(server_fd, 5) == -1) {
        std::cerr << "Listen failed\n";
        close(server_fd);
        return 1;
    }

    std::cout << "Server listening on " << SOCKET_PATH << "...\n";

    int client_fd = accept(server_fd, nullptr, nullptr);
    if (client_fd < 0) {
        std::cerr << "Connection failed\n";
        close(server_fd);
        return 1;
    }

    char buffer[256];
    ssize_t bytes_read;
    while ((bytes_read = read(client_fd, &buffer, sizeof(buffer) - 1)) > 0) {
        buffer[bytes_read] = '\0';
        std::cout << "Received: " << buffer << std::endl;
    }

    close(server_fd);
    close(client_fd);
    unlink(SOCKET_PATH);
    return 0;
}