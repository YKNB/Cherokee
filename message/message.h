#ifndef MESSAGE_H
#define MESSAGE_H

#include <iostream>
#include <string>
#include <sstream>
#include <map>
#include <unordered_map>

// Indicates the processing status of the data in the Request or Response.
enum MSGSTATUS {
    HANDLE_INIT,      // Header data being received/sent (request line, request header)
    HANDLE_HEAD,      // Receiving/Sending Message Header
    HANDLE_BODY,      // Receiving/sending message body
    HANDLE_COMPLETE,  // All data has been processed
    HANDLE_ERROR,     // An error occurred during processing
};

// Indicates the type of message body
enum MSGBODYTYPE {
    FILE_TYPE,      // The message body is the file
    HTML_TYPE,      // The message body is an HTML page
    EMPTY_TYPE,     // Message body is empty
};

// When receiving a file, the message body will be divided into different parts,
// using this type to indicate which part of the file message body has been processed.
enum FILEMSGBODYSTATUS {
    FILE_BEGIN_FLAG,   // The flag line indicating the start of the file is being fetched and processed
    FILE_HEAD,         // Document attributes section being fetched and processed
    FILE_CONTENT,      // Sections of the document whose contents are being captured and processed
    FILE_COMPLETE      // Documentation has been processed
};

// Define the parts of a Request and Response that are common to both, i.e., the message header,
// the message body (you can get a field in the message header and modify the data associated with getting the message body)
class Message {
public:
    Message() : status(HANDLE_INIT) {}

    MSGSTATUS getStatus() const { return status; }
    void setStatus(MSGSTATUS newStatus) { status = newStatus; }

    const std::unordered_map<std::string, std::string>& getHeaders() const { return msgHeader; }
    void setHeader(const std::string& key, const std::string& value) { msgHeader[key] = value; }

protected:
    MSGSTATUS status;                                        // Record the reception status of the message,
                                                             // indicating how much of the entire request message has been received/sent.
    std::unordered_map<std::string, std::string> msgHeader;  // Save Message Header
};

// Inherits Message, modifies and fetches the request line, and saves the received header options.
class Request : public Message {
public:
    Request() : Message(), contentLength(0), msgBodyRecvLen(0), fileMsgStatus(FILE_BEGIN_FLAG) {}

    void setRequestLine(const std::string& requestLine) {
        std::istringstream lineStream(requestLine);
        lineStream >> requestMethod >> requestResource >> httpVersion;
    }

    void addHeaderOpt(const std::string& headLine) {
        std::istringstream lineStream(headLine);
        std::string key, value;

        lineStream >> key;
        key.pop_back();  // Remove colon
        lineStream.get();  // Remove the space after the colon

        getline(lineStream, value);
        if (!value.empty() && value.back() == '\r') {
            value.pop_back();  // Delete the trailing \r
        }

        if (key == "Content-Length") {
            contentLength = std::stoll(value);
        } else if (key == "Content-Type") {
            std::string::size_type semIndex = value.find(';');
            if (semIndex != std::string::npos) {
                msgHeader[key] = value.substr(0, semIndex);
                std::string::size_type eqIndex = value.find('=', semIndex);
                key = value.substr(semIndex + 2, eqIndex - semIndex - 2);
                msgHeader[key] = value.substr(eqIndex + 1);
            } else {
                msgHeader[key] = value;
            }
        } else {
            msgHeader[key] = value;
        }
    }

    const std::string& getRequestMethod() const { return requestMethod; }
    const std::string& getRequestResource() const { return requestResource; }
    const std::string& getHttpVersion() const { return httpVersion; }

    long long getContentLength() const { return contentLength; }
    void setContentLength(long long len) { contentLength = len; }

    long long getMsgBodyRecvLen() const { return msgBodyRecvLen; }
    void setMsgBodyRecvLen(long long len) { msgBodyRecvLen = len; }

    const std::string& getRecvFileName() const { return recvFileName; }
    void setRecvFileName(const std::string& fileName) { recvFileName = fileName; }

    FILEMSGBODYSTATUS getFileMsgStatus() const { return fileMsgStatus; }
    void setFileMsgStatus(FILEMSGBODYSTATUS status) { fileMsgStatus = status; }

    std::string recvMsg;  // Data received but not yet processed

private:
    std::string requestMethod;     // Request Methods for Request Messages
    std::string requestResource;   // Resources requested
    std::string httpVersion;       // HTTP version of the request

    long long contentLength;       // Record the length of the message body
    long long msgBodyRecvLen;      // The length of the message body that has been received

    std::string recvFileName;      // If the client is sending a file, record the name of the file
    FILEMSGBODYSTATUS fileMsgStatus;  // The record indicates what portion of the message body of the file has been processed
};

// Inherit Message, for status line modification and retrieval, set the first option to be sent.
class Response : public Message {
public:
    Response() : Message() {}

    // Getters
    std::string getBodyFileName() const { return bodyFileName; }
    std::string getBeforeBodyMsg() const { return beforeBodyMsg; }
    std::string getMsgBody() const { return msgBody; }
    unsigned long getMsgBodyLen() const { return msgBodyLen; }
    int getBeforeBodyMsgLen() const { return beforeBodyMsgLen; }
    MSGBODYTYPE getBodyType() const { return bodyType; }
    unsigned long getCurStatusHasSendLen() const { return curStatusHasSendLen; }
    int getFileMsgFd() const { return fileMsgFd; }

    // Setters
    void setBodyFileName(const std::string &value) { bodyFileName = value; }
    void setBeforeBodyMsg(const std::string &value) { beforeBodyMsg = value; }
    void setMsgBody(const std::string &value) { msgBody = value; }
    void setMsgBodyLen(unsigned long value) { msgBodyLen = value; }
    void setBeforeBodyMsgLen(int value) { beforeBodyMsgLen = value; }
    void setBodyType(MSGBODYTYPE value) { bodyType = value; }
    void setCurStatusHasSendLen(unsigned long value) { curStatusHasSendLen = value; }
    void setFileMsgFd(int value) { fileMsgFd = value; }

    // Additional Setters for Status Line
    void setResponseHttpVersion(const std::string &value) { responseHttpVersion = value; }
    void setResponseStatusCode(const std::string &value) { responseStatusCode = value; }
    void setResponseStatusDes(const std::string &value) { responseStatusDes = value; }

    // New getter for non-const reference to msgBody
    std::string& getMsgBodyRef() { return msgBody; }

private:
    std::string bodyFileName;      // Path of the data to be sent
    std::string beforeBodyMsg;     // All data before the message body
    std::string msgBody;           // Storing HTML-type message bodies in strings
    unsigned long msgBodyLen;      // Length of the message body
    int beforeBodyMsgLen;          // Length of all data before the message body
    MSGBODYTYPE bodyType;          // Types of messages
    int fileMsgFd;                 // The message body of the file type holds the file descriptor
    unsigned long curStatusHasSendLen;  // Record the length of time this data has been sent in the current state

    // Additional members for Status Line
    std::string responseHttpVersion;
    std::string responseStatusCode;
    std::string responseStatusDes;
};

#endif
