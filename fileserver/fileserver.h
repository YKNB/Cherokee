#ifndef WEBSERVER_H
#define WEBSERVER_H

#include <signal.h>
#include <sys/types.h>
#include <stdexcept>
#include <errno.h>
#include <iostream>
#include <cstring> // For strerror
#include <arpa/inet.h> // For inet_addr and sockaddr_in
#include <unistd.h>
#include <sys/epoll.h>
#include <fcntl.h>  // For fcntl
#include <sys/socket.h>
#include <memory>

#include "../threadpool/threadpool.h"

#define MAX_RESEVENT_SIZE 1024 // Maximum number of events

class WebServer {
public:
    WebServer();
    ~WebServer();

    // Create sockets to wait for clients to connect and turn on listening
    int createListenFd(int port, const char* ip = nullptr);

    // Create epoll routines to listen on sockets
    int createEpoll();

    // Adding a Monitor Listen Socket to epoll
    int epollAddListenFd();

    // Setting up a pipeline to listen to event processing
    int epollAddEventPipe();

    // Setting up TERM and ALARM signal processing
    int addHandleSig(int signo = -1);

    // signal processing function
    static void setSigHandler(int signo);

    // The main thread is responsible for listening to all events
    int waitEpoll();

    // Creating a Thread Pool
    int createThreadPool(int threadNum = 8);

private:
    int m_listenfd;                   // Sockets on the server side
    sockaddr_in m_serverAddr;         // Address information for server-side socket bindings
    static int m_epollfd;             // epoll routine file descriptor for I/O multiplexing
    static bool isStop;               // Whether to suspend the server

    static int eventHandlerPipe[2];   // Pipelines for signaling uniform event sources

    epoll_event resEvents[MAX_RESEVENT_SIZE]; // Array holding results of epoll_wait

    ThreadPool *threadPool;

    void setNonBlocking(int fd);
    int addWaitFd(int epollfd, int fd, bool enableET, bool oneShot);
    static std::string outHead(const std::string &level);
};

#endif
