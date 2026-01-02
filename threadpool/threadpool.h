#ifndef THREADPOOL_H
#define THREADPOOL_H

#include <queue>
#include <stdexcept>
#include <pthread.h>
#include <semaphore.h>
#include <vector>
#include <iostream>
#include <cstring> 
#include "../event/myevent.h"

class ThreadPool {
public:
    ThreadPool(int threadNum);
    ~ThreadPool();

    // Adds a pending event to the event queue, and threads in the thread pool will loop through it to process the event
    int appendEvent(EventBase* event, const std::string& eventType);

private:
    static void* worker(void* arg);
    void run();
    std::string outHead(const std::string& level);

    int m_threadNum;                  
    std::vector<pthread_t> m_threads; 
    std::queue<EventBase*> m_workQueue;  
    pthread_mutex_t queueLocker;    
    sem_t queueEventNum;             
};

#endif
