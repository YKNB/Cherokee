#include "fileserver.h"

int WebServer::m_epollfd = -1;
bool WebServer::isStop = false;
int WebServer::eventHandlerPipe[2] = {-1, -1};

WebServer::WebServer() : m_listenfd(-1), threadPool(nullptr) {}

WebServer::~WebServer() {
    if (m_listenfd != -1) {
        close(m_listenfd);
    }
    if (threadPool) {
        delete threadPool;
    }
}

int WebServer::createListenFd(int port, const char* ip) {
    bzero(&m_serverAddr, sizeof(m_serverAddr));
    m_serverAddr.sin_family = AF_INET;
    m_serverAddr.sin_port = htons(port);
    
    if (ip != nullptr) {
        m_serverAddr.sin_addr.s_addr = inet_addr(ip);
    } else {
        m_serverAddr.sin_addr.s_addr = htonl(INADDR_ANY);
    }

    m_listenfd = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (m_listenfd < 0) {
        throw std::runtime_error("Socket creation failure: " + std::string(strerror(errno)));
    }

    int reuseAddr = 1;
    if (setsockopt(m_listenfd, SOL_SOCKET, SO_REUSEADDR, &reuseAddr, sizeof(reuseAddr)) < 0) {
        close(m_listenfd);
        throw std::runtime_error("Socket set address reuse failed: " + std::string(strerror(errno)));
    }

    if (bind(m_listenfd, (sockaddr*)&m_serverAddr, sizeof(m_serverAddr)) < 0) {
        close(m_listenfd);
        throw std::runtime_error("Socket binding address failure: " + std::string(strerror(errno)));
    }

    if (listen(m_listenfd, 5) < 0) {
        close(m_listenfd);
        throw std::runtime_error("Socket open listening failed: " + std::string(strerror(errno)));
    }

    return 0;
}

int WebServer::createEpoll() {
    m_epollfd = epoll_create(100);
    if (m_epollfd < 0) {
        throw std::runtime_error("Failed to create epoll: " + std::string(strerror(errno)));
    }
    return 0;
}

int WebServer::epollAddListenFd() {
    setNonBlocking(m_listenfd);
    int ret = addWaitFd(m_epollfd, m_listenfd, true, false);
    if (ret != 0) {
        throw std::runtime_error("Add Monitor Listen Socket Failed: " + std::string(strerror(errno)));
    }
    std::cout << outHead("info") << "Successfully added a listening socket to epoll." << std::endl;
    return 0;
}

int WebServer::epollAddEventPipe() {
    if (socketpair(PF_UNIX, SOCK_STREAM, 0, eventHandlerPipe) != 0) {
        throw std::runtime_error("Failed to create bi-directional pipeline: " + std::string(strerror(errno)));
    }
    setNonBlocking(eventHandlerPipe[0]);
    setNonBlocking(eventHandlerPipe[1]);
    if (addWaitFd(m_epollfd, eventHandlerPipe[0], true, false) != 0) {
        throw std::runtime_error("Add monitor pipe[0] failed: " + std::string(strerror(errno)));
    }
    return 0;
}

int WebServer::addHandleSig(int signo) {
    struct sigaction act;
    act.sa_handler = setSigHandler;
    sigfillset(&act.sa_mask);
    act.sa_flags = 0;

    if (signo == -1) {
        const int signals[] = {SIGINT, SIGTERM, SIGALRM};
        for (int signal : signals) {
            if (sigaction(signal, &act, nullptr) < 0) {
                throw std::runtime_error("Failed to set signal handler for signal: " + std::to_string(signal));
            }
        }
    } else {
        if (sigaction(signo, &act, nullptr) < 0) {
            throw std::runtime_error("Failed to set signal handler for signal: " + std::to_string(signo));
        }
    }
    return 0;
}

void WebServer::setSigHandler(int signo) {
    if ((signo & SIGINT) || (signo & SIGTERM)) {
        isStop = true;
    }
    int saveErrno = errno;
    int msg = signo;
    if (send(eventHandlerPipe[1], &msg, sizeof(msg), 0) != sizeof(msg)) {
        std::cout << outHead("error") << "Signal processing failure" << std::endl;
    }
    errno = saveErrno;
}

int WebServer::waitEpoll() {
    isStop = false;

    std::unique_ptr<EventBase> event;

    while (!isStop) {
        int resNum = epoll_wait(m_epollfd, resEvents, MAX_RESEVENT_SIZE, -1);
        if (resNum < 0 && errno != EINTR) {
            throw std::runtime_error("epoll_wait execution error: " + std::string(strerror(errno)));
        }
        for (int i = 0; i < resNum; ++i) {
            int resfd = resEvents[i].data.fd;
            if (resfd == m_listenfd) {
                event.reset(new AcceptConn(m_listenfd, m_epollfd));
            } else if ((resfd == eventHandlerPipe[0]) && (resEvents[i].events & EPOLLIN)) {
                // Handle signaling events
                continue;
            } else if (resEvents[i].events & EPOLLIN) {
                event.reset(new HandleRecv(resEvents[i].data.fd, m_epollfd));
            } else if (resEvents[i].events & EPOLLOUT) {
                event.reset(new HandleSend(resEvents[i].data.fd, m_epollfd));
            }
            if (event) {
                threadPool->appendEvent(event.release(), "event");
            }
        }
    }
    return 0;
}

int WebServer::createThreadPool(int threadNum) {
    try {
        threadPool = new ThreadPool(threadNum);
    } catch (std::runtime_error &err) {
        std::cout << err.what() << std::endl;
    }
    if (!threadPool) {
        throw std::runtime_error("Thread pool creation failed");
    }
    return 0;
}

void WebServer::setNonBlocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    if (flags == -1) {
        throw std::runtime_error("Failed to get file descriptor flags: " + std::string(strerror(errno)));
    }
    if (fcntl(fd, F_SETFL, flags | O_NONBLOCK) == -1) {
        throw std::runtime_error("Failed to set file descriptor non-blocking: " + std::string(strerror(errno)));
    }
}

int WebServer::addWaitFd(int epollfd, int fd, bool enableET, bool oneShot) {
    epoll_event ev;
    ev.data.fd = fd;
    ev.events = EPOLLIN | EPOLLET;
    if (oneShot) {
        ev.events |= EPOLLONESHOT;
    }
    if (epoll_ctl(epollfd, EPOLL_CTL_ADD, fd, &ev) == -1) {
        throw std::runtime_error("Failed to add file descriptor to epoll: " + std::string(strerror(errno)));
    }
    return 0;
}

std::string WebServer::outHead(const std::string &level) {
    return "[" + level + "] ";
}
