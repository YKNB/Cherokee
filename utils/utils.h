#ifndef UTILS_H
#define UTILS_H
#pragma once
#include <iostream>
#include <ctime>
#include <chrono>
#include <string>
#include <sys/time.h>
#include <sys/epoll.h>
#include <fcntl.h>
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/types.h>
#include <semaphore.h>
#include <search.h>

// Taille de la mémoire partagée
#define SHM_SIZE 1024  

// Fonctions pour gérer la mémoire partagée
key_t get_shm_key(const char *path, int id);
int init_shared_memory(key_t key, size_t size);
void* attach_shared_memory(int shmid);
void detach_shared_memory(void* shmaddr);
void destroy_shared_memory(int shmid);

// Fonctions pour gérer la table de hachage
struct hsearch_data* init_hash_table(size_t size);
void destroy_hash_table(struct hsearch_data* htab);
bool insert_into_hash_table(struct hsearch_data* htab, const char* key, const char* value);
const char* find_in_hash_table(struct hsearch_data* htab, const char* key);

// Fonctions pour gérer les sémaphores
void init_semaphores();
void wait_semaphore(sem_t* sem);
void post_semaphore(sem_t* sem);

extern sem_t cache_sem;  // Déclaration externe de cache_sem

// Fonctions existantes
std::string outHead(const std::string& logType);
int addWaitFd(int epollFd, int newFd, bool edgeTrigger = false, bool isOneshot = false);
int modifyWaitFd(int epollFd, int modFd, bool edgeTrigger = false, bool resetOneshot = false, bool addEpollout = false);
int deleteWaitFd(int epollFd, int deleteFd);
int setNonBlocking(int fd);

#endif
