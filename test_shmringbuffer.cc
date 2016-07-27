#include <iostream>
#include <cstdio>
#include <unistd.h>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include "shmringbuffer.hh"

struct LogNode {
    int ts;  // timestamp
    int len; // length
#define MAX_LOG_LEN 256
    char log[MAX_LOG_LEN];

    const std::string unparse() {
        return "[" + std::to_string(ts) + "] " + std::string(&log[0]);
    }
};

int main()
{
    /* initialize random seed: */
    srand (time(NULL));

    const int CAPACITY = 20;
    pid_t pid = fork();
    if (pid == 0) {
        // child process
        usleep(500);
        ShmRingBuffer<LogNode> buffer(CAPACITY, false);
        int start = 1000;
        LogNode log;
        for (int i = start; i < start + 10*CAPACITY; ++i) {
            snprintf(log.log, MAX_LOG_LEN, "%zu: %d", buffer.end(), i);
            buffer.push_back(log);
            std::cout << "child: insert " << i << ", index " << buffer.end() << std::endl; // FIXME
            usleep(rand()%1000+500);
        }
        exit(0);
    } else if (pid > 0) {
        // parent process
        ShmRingBuffer<LogNode> buffer(CAPACITY, true);
        int start = 2000;
        LogNode log;
        for (int i = start; i < start + 10*CAPACITY; ++i) {
            snprintf(log.log, MAX_LOG_LEN, "%zu: %d", buffer.end(), i);
            buffer.push_back(log);
            std::cout << "parent: insert " << i << ", index " << buffer.end() << std::endl; // FIXME
            usleep(rand()%900+500);
        }

        usleep(5000); // wait for child process exit
        std::cout << "Ring Buffer:" << std::endl;
        std::cout << buffer.unparse() << std::endl;
    } else {
        // fork failed
        std::cout << "fork() failed." << std::endl;
        return 1;
    }

    return 0;
}
