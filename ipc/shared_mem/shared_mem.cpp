#include "common.hpp"

#include "sys/mman.h"
#include "sys/wait.h"
#include "fcntl.h"
#include "unistd.h"

#include <iostream>
#include <string>
#include <cstring>

const int MESSAGE_COUNT = 5;
const char* END_MESSAGE = "----END----";

int main() {
    int fd = shm_open(SHM_NAME, O_CREAT | O_RDWR, 0600);
    if (fd == -1) {
        std::cerr << "Shared memory creation failed\n";
        return 1;
    }

    if (ftruncate(fd, sizeof(SharedData)) == -1) {
        std::cerr << "Setting shared memory size failed\n";
        return 1;
    }

    void* ptr = mmap(NULL, sizeof(SharedData), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (ptr == MAP_FAILED) {
        std::cerr << "Memory map failed\n";
        return 1;
    }

    SharedData* data = reinterpret_cast<SharedData*>(ptr);
    sem_init(&data->writer_sem, 1, 1);
    sem_init(&data->reader_sem, 1, 0);

    pid_t pid = fork();
    if (pid < 0) {
        std::cerr << "Fork failed\n";
        return 1;
    }

    if (pid == 0) {
        
        bool is_running = true;
        while (is_running) {
            sem_wait(&data->reader_sem);

            std::cout << "Received: " << data->message << '\n';
            if (strcmp(END_MESSAGE, data->message) == 0) {
                is_running = false;
            }

            sem_post(&data->writer_sem);
        }
        
        
        munmap(ptr, sizeof(SharedData));
        close(fd);

    } else {
        for (int i = 0; i < MESSAGE_COUNT; i++) {
            sem_wait(&data->writer_sem);
            
            if (i == MESSAGE_COUNT - 1) {
                strncpy(data->message, END_MESSAGE, sizeof(data->message));
            } else {
                std::string msg = "Message " + std::to_string(i);
                strncpy(data->message, msg.c_str(), sizeof(data->message));
            }

            sem_post(&data->reader_sem);
        }
        
        int status;
        waitpid(pid, &status, 0);

        sem_destroy(&data->writer_sem);
        sem_destroy(&data->reader_sem);

        munmap(ptr, sizeof(SharedData));
        close(fd);
        shm_unlink(SHM_NAME);
    }
}
