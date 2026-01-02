#ifndef MYEVENT_H
#define MYEVENT_H

#include <dirent.h>
#include <fstream>
#include <vector>
#include <cstdio>
#include <sstream>
#include <algorithm>
#include <sys/stat.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <sys/sendfile.h>
#include <unistd.h>
#include <unordered_map>
#include <string>

#include "../message/message.h"
#include "../utils/utils.h"

// Base class for all events
class EventBase {
public:
    EventBase() = default;
    virtual ~EventBase() = default;

    // Override this function for different types of events to perform different handlers
    virtual void process() = 0;

protected:
    // Saves the state of the request corresponding to the file descriptor, 
    // since the data on a connection may not be non-blocking enough to read all at once,
    // so it is saved here and can continue to be read and processed when there is new data on that connection
    static std::unordered_map<int, Request> requestStatus;

    // Save the state of the file descriptor corresponding to the sent data,
    // a process non-blocking write data may not be able to pass all the data,
    // so save the current state of the data sent, you can continue to pass the data
    static std::unordered_map<int, Response> responseStatus;
};

// Events for accepting client connections
class AcceptConn : public EventBase {
public:
    AcceptConn(int listenFd, int epollFd);
    virtual ~AcceptConn() = default;

    virtual void process() override;

private:
    int m_listenFd;    // Save listening sockets 
    int m_epollFd;     // The epoll that was added after receiving the connection
    int accetpFd;      // Save accepted connections

    sockaddr_in clientAddr;  // client address
    socklen_t clientAddrLen; // Client address length
};

// Used to deal with the events of the signal, no need to deal with the events of the signal for the time being,
// there is no specific realization of the realization
class HandleSig : public EventBase {
public:
    explicit HandleSig(int epollFd);
    virtual ~HandleSig() = default;

    virtual void process() override;
};

// Processing requests sent by the client
class HandleRecv : public EventBase {
public:
    HandleRecv(int clientFd, int epollFd);
    virtual ~HandleRecv() = default;

    virtual void process() override;

private:
    int m_clientFd;   // Client socket to read data from that client
    int m_epollFd;    // epoll file descriptor, used when you need to reset an event or close a connection
};

// Handles sending data to the client
class HandleSend : public EventBase {
public:
    HandleSend(int clientFd, int epollFd);
    virtual ~HandleSend() = default;

    virtual void process() override;
    
    // Used to construct the status line, the parameters represent each of the three parts of the status line
    std::string getStatusLine(const std::string& httpVersion, const std::string& statusCode, const std::string& statusDes);

    // The following two functions are used to build the file list page, and the final result is saved in fileListHtml.
    void getFileListPage(std::string& fileListHtml);

    void getFileVec(const std::string& dirName, std::vector<std::string>& resVec);

    // Constructing header fields：
    // contentLength        : Specifies the length of the message body
    // contentType          : Specify the type of message body
    // redirectLocation = "" : In the case of a redirected message, you can specify the address of the redirection. An empty string indicates that this initialization is not added。
    // contentRange = ""    : If this is a response message for downloading a file, specify the range of files currently being sent. An empty string indicates that this initialization is not added。
    std::string getMessageHeader(const std::string& contentLength, const std::string& contentType, const std::string& redirectLocation = "", const std::string& contentRange = "");

private:
    int m_clientFd;   // Client socket to write data to this client
    int m_epollFd;    // epoll file descriptor, used when you need to reset an event or close a connection
};

#endif
