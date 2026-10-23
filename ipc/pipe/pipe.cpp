#include "unistd.h"
#include <iostream>

int main() {
    int fd[2];
    pipe(fd);

    pid_t pid = fork();
    if (pid < 0) {
        std::cerr << "Fork failed\n";
        return 1;
    }

    if (pid == 0) {
        close(fd[1]);
        char buffer[64];
        int n = read(fd[0], &buffer, sizeof(buffer));
        close(fd[0]);
        write(STDOUT_FILENO, &buffer, n);
        return 0;
    } else {
        close(fd[0]);
        write(fd[1], "Hello\n", 6);
        close(fd[1]);
        return 0;
    }
}