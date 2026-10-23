#pragma once

#include "semaphore.h"

const char* SHM_NAME = "/shared_mem_example";

struct SharedData {
    sem_t writer_sem;
    sem_t reader_sem;
    char message[256];
};
