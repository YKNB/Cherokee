#include "./fileserver/fileserver.h"
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <unistd.h>
#include <iostream>
#include <search.h>
#include "utils/utils.h"
#include <semaphore.h>

void cache_manager() {
    key_t key = get_shm_key("shmfile", 65);
    int shmid = init_shared_memory(key, SHM_SIZE);
    char* str = static_cast<char*>(attach_shared_memory(shmid));

    struct hsearch_data* htab = init_hash_table(50);

    init_semaphores();

    while (true) {
        wait_semaphore(&cache_sem);

        // Gérer les requêtes pour ajouter ou récupérer des éléments du cache

        post_semaphore(&cache_sem);
    }

    detach_shared_memory(str);
    destroy_shared_memory(shmid);
    destroy_hash_table(htab);
}

int main() {
    pid_t pid = fork();

    if (pid == 0) {
        // Processus enfant : gestionnaire de cache
        cache_manager();
    } else if (pid > 0) {
        // Processus parent : serveur web
        WebServer webserver;

        init_semaphores();

        // Creating a Thread Pool
        int ret = webserver.createThreadPool(4);
        if(ret != 0){
            std::cout << outHead("error") << "Failed to create thread pool" << std::endl;
            return -1;
        }

        // Initialize sockets for listening
        int port = 8888;
        ret = webserver.createListenFd(port);
        if(ret != 0){
            std::cout << outHead("error") << "Failed to create and initialize listening socket" << std::endl;
            return -2;
        }

        // The epoll routine that initializes the listener
        ret = webserver.createEpoll();
        if(ret != 0){
            std::cout << outHead("error") << "Failure to initialize listening epoll routine" << std::endl;
            return -3;
        }

        // Adding a listening socket to epoll
        ret = webserver.epollAddListenFd();
        if(ret != 0){
            std::cout << outHead("error") << "epoll failed to add listening socket" << std::endl;
            return -4;
        }

        // Enables listening and processing of requests
        ret = webserver.waitEpoll();
        if(ret != 0){
            std::cout << outHead("error") << "epoll routine listening failure" << std::endl;
            return -5;
        }
    } else {
        std::cerr << "Failed to fork" << std::endl;
        return 1;
    }

    return 0;
}
