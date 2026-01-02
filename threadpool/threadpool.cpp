#include "threadpool.h"

ThreadPool::ThreadPool(int threadNum) : m_threadNum(threadNum), m_threads(threadNum) {
    int ret = pthread_mutex_init(&queueLocker, nullptr);
    if (ret != 0) {
        throw std::runtime_error("Failed to initialize mutex: " + std::string(strerror(errno)));
    }

    ret = sem_init(&queueEventNum, 0, 0);
    if (ret != 0) {
        pthread_mutex_destroy(&queueLocker);
        throw std::runtime_error("Failed to initialize semaphore: " + std::string(strerror(errno)));
    }

    for (int i = 0; i < m_threadNum; ++i) {
        ret = pthread_create(&m_threads[i], nullptr, worker, this);
        if (ret != 0) {
            pthread_mutex_destroy(&queueLocker);
            sem_destroy(&queueEventNum);
            throw std::runtime_error("Thread creation failure: " + std::string(strerror(errno)));
        }
        ret = pthread_detach(m_threads[i]);
        if (ret != 0) {
            pthread_mutex_destroy(&queueLocker);
            sem_destroy(&queueEventNum);
            throw std::runtime_error("Failed to set up detached thread: " + std::string(strerror(errno)));
        }
    }
}

ThreadPool::~ThreadPool() {
    pthread_mutex_destroy(&queueLocker);
    sem_destroy(&queueEventNum);
}

int ThreadPool::appendEvent(EventBase* event, const std::string& eventType) {
    int ret = pthread_mutex_lock(&queueLocker);
    if (ret != 0) {
        std::cout << outHead("error") << "Event queue lock failure" << std::endl;
        return -1;
    }

    m_workQueue.push(event);
    std::cout << outHead("info") << eventType << " successfully added, number of events remaining in the thread pool event queue: " << m_workQueue.size() << std::endl;

    ret = pthread_mutex_unlock(&queueLocker);
    if (ret != 0) {
        std::cout << outHead("error") << "Failed to unlock event queue" << std::endl;
        return -2;
    }

    ret = sem_post(&queueEventNum);
    if (ret != 0) {
        std::cout << outHead("error") << "Event queue semaphore post failed" << std::endl;
        return -3;
    }

    return 0;
}

void* ThreadPool::worker(void* arg) {
    ThreadPool* thiz = static_cast<ThreadPool*>(arg);
    thiz->run();
    return nullptr;
}

void ThreadPool::run() {
    while (true) {
        int ret = sem_wait(&queueEventNum);
        if (ret != 0) {
            std::cout << outHead("error") << "Waiting for queue events to fail" << std::endl;
            return;
        }

        ret = pthread_mutex_lock(&queueLocker);
        if (ret != 0) {
            std::cout << outHead("error") << "ThreadPool::run() : Event queue lock failure" << std::endl;
            return;
        }

        if (m_workQueue.empty()) {
            pthread_mutex_unlock(&queueLocker);
            continue;
        }

        EventBase* curEvent = m_workQueue.front();
        m_workQueue.pop();

        ret = pthread_mutex_unlock(&queueLocker);
        if (ret != 0) {
            std::cout << outHead("error") << "ThreadPool::run() : Failed to unlock event queue" << std::endl;
            return;
        }

        if (curEvent == nullptr) {
            continue;
        }

        curEvent->process();
        delete curEvent;
    }
}

std::string ThreadPool::outHead(const std::string& level) {
    return "[" + level + "] ";
}
