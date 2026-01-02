#include "utils.h"
#include <cstring>  

key_t get_shm_key(const char *path, int id) {
    return ftok(path, id);
}

int init_shared_memory(key_t key, size_t size) {
    return shmget(key, size, 0666 | IPC_CREAT);
}

void* attach_shared_memory(int shmid) {
    return shmat(shmid, nullptr, 0);
}

void detach_shared_memory(void* shmaddr) {
    shmdt(shmaddr);
}

void destroy_shared_memory(int shmid) {
    shmctl(shmid, IPC_RMID, nullptr);
}

struct hsearch_data* init_hash_table(size_t size) {
    auto* htab = new hsearch_data;
    memset(htab, 0, sizeof(hsearch_data));
    if (hcreate_r(size, htab) == 0) {
        delete htab;
        return nullptr;
    }
    return htab;
}

void destroy_hash_table(struct hsearch_data* htab) {
    if (htab) {
        hdestroy_r(htab);
        delete htab;
    }
}

bool insert_into_hash_table(struct hsearch_data* htab, const char* key, const char* value) {
    ENTRY e;
    e.key = const_cast<char*>(key);
    e.data = const_cast<char*>(value);
    ENTRY* ep;
    return hsearch_r(e, ENTER, &ep, htab) != 0;
}

const char* find_in_hash_table(struct hsearch_data* htab, const char* key) {
    ENTRY e;
    e.key = const_cast<char*>(key);
    ENTRY* ep;
    if (hsearch_r(e, FIND, &ep, htab) != 0) {
        return static_cast<const char*>(ep->data);
    }
    return nullptr;
}

sem_t cache_sem;  // Définition de cache_sem

void init_semaphores() {
    if (sem_init(&cache_sem, 1, 1) != 0) {
        std::cerr << "Failed to initialize semaphore" << std::endl;
        exit(1);
    }
}

void wait_semaphore(sem_t* sem) {
    sem_wait(sem);
}

void post_semaphore(sem_t* sem) {
    sem_post(sem);
}

std::string outHead(const std::string& logType) {
    auto now = std::chrono::system_clock::now();
    time_t tt = std::chrono::system_clock::to_time_t(now);
    auto time_tm = localtime(&tt);

    struct timeval time_usec;
    gettimeofday(&time_usec, nullptr);

    char strTime[30];
    snprintf(strTime, sizeof(strTime), "%02d:%02d:%02d.%05ld %d-%02d-%02d",
             time_tm->tm_hour, time_tm->tm_min, time_tm->tm_sec, time_usec.tv_usec,
             time_tm->tm_year + 1900, time_tm->tm_mon + 1, time_tm->tm_mday);

    std::string outStr = strTime;
    if (logType == "init") {
        outStr += " [init]: ";
    } else if (logType == "error") {
        outStr += " [erro]: ";
    } else {
        outStr += " [info]: ";
    }

    return outStr;
}

int addWaitFd(int epollFd, int newFd, bool edgeTrigger, bool isOneshot) {
    epoll_event event;
    event.data.fd = newFd;
    event.events = EPOLLIN;

    if (edgeTrigger) {
        event.events |= EPOLLET;
    }
    if (isOneshot) {
        event.events |= EPOLLONESHOT;
    }

    int ret = epoll_ctl(epollFd, EPOLL_CTL_ADD, newFd, &event);
    if (ret != 0) {
        std::cerr << outHead("error") << "Failed to add file descriptor" << std::endl;
        return -1;
    }
    return 0;
}

int modifyWaitFd(int epollFd, int modFd, bool edgeTrigger, bool resetOneshot, bool addEpollout) {
    epoll_event event;
    event.data.fd = modFd;
    event.events = EPOLLIN;

    if (edgeTrigger) {
        event.events |= EPOLLET;
    }
    if (resetOneshot) {
        event.events |= EPOLLONESHOT;
    }
    if (addEpollout) {
        event.events |= EPOLLOUT;
    }

    int ret = epoll_ctl(epollFd, EPOLL_CTL_MOD, modFd, &event);
    if (ret != 0) {
        std::cerr << outHead("error") << "Failed to modify file descriptor" << std::endl;
        return -1;
    }
    return 0;
}

int deleteWaitFd(int epollFd, int deleteFd) {
    int ret = epoll_ctl(epollFd, EPOLL_CTL_DEL, deleteFd, nullptr);
    if (ret != 0) {
        std::cerr << outHead("error") << "Failed to remove listening file descriptor" << std::endl;
        return -1;
    }
    return 0;
}

int setNonBlocking(int fd) {
    int oldFlag = fcntl(fd, F_GETFL);
    int ret = fcntl(fd, F_SETFL, oldFlag | O_NONBLOCK);
    return (ret == 0) ? 0 : -1;
}
