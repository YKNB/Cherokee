#include "myevent.h"

// Utility function to remove spaces from filenames
std::string removeSpaces(const std::string &str) {
    std::string result;
    for (char c : str) {
        if (!isspace(c)) {
            result += c;
        }
    }
    return result;
}

// Out-of-class initialization of static members
std::unordered_map<int, Request> EventBase::requestStatus;
std::unordered_map<int, Response> EventBase::responseStatus;

AcceptConn::AcceptConn(int listenFd, int epollFd) : m_listenFd(listenFd), m_epollFd(epollFd) {}

void AcceptConn::process() {
    // accept a connection
    clientAddrLen = sizeof(clientAddr);
    accetpFd = accept(m_listenFd, (sockaddr*)&clientAddr, &clientAddrLen);
    if (accetpFd == -1) {
        std::cout << "[error] Failed to accept new connection" << std::endl;
        return;
    }

    // Setting the connection to non-blocking
    setNonBlocking(accetpFd);

    // The connection is added to the listener, and the client sockets are both set to EPOLLET and EPOLLONESHOT.
    addWaitFd(m_epollFd, accetpFd, true, true);
    std::cout << "[info] Accepting new connections " << accetpFd << " successes" << std::endl;
}

HandleRecv::HandleRecv(int clientFd, int epollFd) : m_clientFd(clientFd), m_epollFd(epollFd) {}

void HandleRecv::process() {
    std::cout << "[info] Starting client processing " << m_clientFd << " A HandleRecv event of the" << std::endl;
    requestStatus[m_clientFd];

    char buf[2048];
    int recvLen = 0;

    while (1) {
        recvLen = recv(m_clientFd, buf, 2048, 0);

        if (recvLen == 0) {
            std::cout << "[info] client (computing) " << m_clientFd << " Close connection" << std::endl;
            requestStatus[m_clientFd].setStatus(HANDLE_ERROR);
            break;
        }

        if (recvLen == -1) {
            if (errno != EAGAIN) {
                requestStatus[m_clientFd].setStatus(HANDLE_ERROR);
                std::cout << "[error] Returned when receiving data -1 (errno = " << errno << ")" << std::endl;
                break;
            }
            modifyWaitFd(m_epollFd, m_clientFd, true, true, false);
            break;
        }

        requestStatus[m_clientFd].recvMsg.append(buf, recvLen);

        std::string::size_type endIndex = 0;

        if (requestStatus[m_clientFd].getStatus() == HANDLE_INIT) {
            endIndex = requestStatus[m_clientFd].recvMsg.find("\r\n");

            if (endIndex != std::string::npos) {
                requestStatus[m_clientFd].setRequestLine(requestStatus[m_clientFd].recvMsg.substr(0, endIndex + 2));
                requestStatus[m_clientFd].recvMsg.erase(0, endIndex + 2);
                requestStatus[m_clientFd].setStatus(HANDLE_HEAD);
                std::cout << "[info] Processing Clients " << m_clientFd << " The request line is completed" << std::endl;
            }
        }

        if (requestStatus[m_clientFd].getStatus() == HANDLE_HEAD) {
            std::string curLine;

            while (1) {
                endIndex = requestStatus[m_clientFd].recvMsg.find("\r\n");
                if (endIndex == std::string::npos) {
                    break;
                }

                curLine = requestStatus[m_clientFd].recvMsg.substr(0, endIndex + 2);
                requestStatus[m_clientFd].recvMsg.erase(0, endIndex + 2);

                if (curLine == "\r\n") {
                    requestStatus[m_clientFd].setStatus(HANDLE_BODY);
                    if (requestStatus[m_clientFd].getHeaders().find("Content-Type") != requestStatus[m_clientFd].getHeaders().end() &&
                        requestStatus[m_clientFd].getHeaders().at("Content-Type") == "multipart/form-data") {
                        requestStatus[m_clientFd].setFileMsgStatus(FILE_BEGIN_FLAG);
                    }
                    std::cout << "[info] Processing Clients " << m_clientFd << " The message header of the" << std::endl;
                    if (requestStatus[m_clientFd].getRequestMethod() == "POST") {
                        std::cout << "[info] client (computing) " << m_clientFd << " Send a POST request to start processing the request body" << std::endl;
                    }
                    break;
                }

                requestStatus[m_clientFd].addHeaderOpt(curLine);
            }
        }

        if (requestStatus[m_clientFd].getStatus() == HANDLE_BODY) {
            if (requestStatus[m_clientFd].getRequestMethod() == "GET") {
                responseStatus[m_clientFd].setBodyFileName(requestStatus[m_clientFd].getRequestResource());
                modifyWaitFd(m_epollFd, m_clientFd, true, true, true);
                requestStatus[m_clientFd].setStatus(HANDLE_COMPLETE);
                std::cout << "[info] client (computing) " << m_clientFd << " Sending a GET request, the requested resource has been composed into a Response Write event waiting to send data." << std::endl;
                break;
            }

            if (requestStatus[m_clientFd].getRequestMethod() == "POST") {
                std::string::size_type beginSize = requestStatus[m_clientFd].recvMsg.size();
                if (requestStatus[m_clientFd].getHeaders().find("Content-Type") != requestStatus[m_clientFd].getHeaders().end() &&
                    requestStatus[m_clientFd].getHeaders().at("Content-Type") == "multipart/form-data") {
                    if (requestStatus[m_clientFd].getFileMsgStatus() == FILE_BEGIN_FLAG) {
                        std::cout << "[info] client (computing) " << m_clientFd << " The POST request is used to upload a file, looking for the file header start boundary..." << std::endl;
                        endIndex = requestStatus[m_clientFd].recvMsg.find("\r\n");

                        if (endIndex != std::string::npos) {
                            std::string flagStr = requestStatus[m_clientFd].recvMsg.substr(0, endIndex);

                            if (flagStr == "--" + requestStatus[m_clientFd].getHeaders().at("boundary")) {
                                requestStatus[m_clientFd].setFileMsgStatus(FILE_HEAD);
                                requestStatus[m_clientFd].recvMsg.erase(0, endIndex + 2);
                                std::cout << "[info] client (computing) " << m_clientFd << " The header start boundary is found in the body of the POST request for the file header being processed..." << std::endl;
                            } else {
                                responseStatus[m_clientFd].setBodyFileName("/redirect");
                                modifyWaitFd(m_epollFd, m_clientFd, true, true, true);
                                requestStatus[m_clientFd].setStatus(HANDLE_COMPLETE);
                                std::cout << "[error] client (computing) " << m_clientFd << " in the body of a POST request that does not find a file header start boundary, add a Redirect Response Write event to redirect the client to the file list" << std::endl;
                                break;
                            }
                        }
                    }

                    if (requestStatus[m_clientFd].getFileMsgStatus() == FILE_HEAD) {
                        std::string strLine;
                        while (1) {
                            endIndex = requestStatus[m_clientFd].recvMsg.find("\r\n");
                            if (endIndex != std::string::npos) {
                                strLine = requestStatus[m_clientFd].recvMsg.substr(0, endIndex + 2);
                                requestStatus[m_clientFd].recvMsg.erase(0, endIndex + 2);

                                if (strLine == "\r\n") {
                                    requestStatus[m_clientFd].setFileMsgStatus(FILE_CONTENT);
                                    std::cout << "[info] client (computing) " << m_clientFd << " The file header in the body of the POST request was processed successfully, and the contents of the file are being received and saved..." << std::endl;
                                    break;
                                }
                                endIndex = strLine.find("filename");
                                if (endIndex != std::string::npos) {
                                    strLine.erase(0, endIndex + std::string("filename=\"").size());
                                    for (int i = 0; strLine[i] != '\"'; ++i) {
                                        requestStatus[m_clientFd].setRecvFileName(requestStatus[m_clientFd].getRecvFileName() + strLine[i]);
                                    }
                                    requestStatus[m_clientFd].setRecvFileName(removeSpaces(requestStatus[m_clientFd].getRecvFileName()));
                                    std::cout << "[info] client (computing) " << m_clientFd << " to find the file name in the body of the POST request for the " << requestStatus[m_clientFd].getRecvFileName() << " The header of the document continues to be processed..." << std::endl;
                                }
                            } else {
                                break;
                            }
                        }
                    }

                    if (requestStatus[m_clientFd].getFileMsgStatus() == FILE_CONTENT) {
                        std::ofstream ofs("filedir/" + requestStatus[m_clientFd].getRecvFileName(), std::ios::out | std::ios::app | std::ios::binary);
                        if (!ofs) {
                            std::cout << "[error] client (computing) " << m_clientFd << " The file to be saved in the body of the POST request failed to open and is being reopened..." << std::endl;
                            break;
                        }

                        while (1) {
                            int saveLen = requestStatus[m_clientFd].recvMsg.size();
                            if (saveLen == 0) {
                                break;
                            }
                            endIndex = requestStatus[m_clientFd].recvMsg.find('\r');
                                        
                            if (endIndex != std::string::npos) {
                                int boundarySecLen = requestStatus[m_clientFd].getHeaders().at("boundary").size() + 8;
                                if (requestStatus[m_clientFd].recvMsg.size() - endIndex >= boundarySecLen) {
                                    if (requestStatus[m_clientFd].recvMsg.substr(endIndex, boundarySecLen) == "\r\n--" + requestStatus[m_clientFd].getHeaders().at("boundary") + "--\r\n") {
                                        if (endIndex == 0) {
                                            std::cout << "[info] client (computing) " << m_clientFd << " The file data in the body of the POST request is received and saved." << std::endl;
                                            requestStatus[m_clientFd].setFileMsgStatus(FILE_COMPLETE);
                                            break;
                                        }
                                        saveLen = endIndex;
                                    } else {
                                        endIndex = requestStatus[m_clientFd].recvMsg.find('\r', endIndex + 1);
                                        if (endIndex != std::string::npos) {
                                            saveLen = endIndex;
                                        }
                                    }
                                } else {
                                    if (endIndex == 0) {
                                        break;
                                    }
                                    saveLen = endIndex;
                                }
                            }
                            ofs.write(requestStatus[m_clientFd].recvMsg.c_str(), saveLen);
                            requestStatus[m_clientFd].recvMsg.erase(0, saveLen);
                        }
                        ofs.close();
                    }

                    if (requestStatus[m_clientFd].getFileMsgStatus() == FILE_COMPLETE) {
                        responseStatus[m_clientFd].setBodyFileName("/redirect");
                        modifyWaitFd(m_epollFd, m_clientFd, true, true, true);
                        requestStatus[m_clientFd].setStatus(HANDLE_COMPLETE);
                        std::cout << "[info] client (computing) " << m_clientFd << " The POST request body is processed, a Response write event is added, and a redirect message is sent to refresh the file list." << std::endl;
                        break;
                    }
                } else {
                    responseStatus[m_clientFd].setBodyFileName("/redirect");
                    modifyWaitFd(m_epollFd, m_clientFd, true, true, true);
                    requestStatus[m_clientFd].setStatus(HANDLE_COMPLETE);
                    std::cout << "[error] client (computing) " << m_clientFd << "If you receive data in a POST request that cannot be processed, add a Response write event that returns a message redirecting to the file list." << std::endl;
                    break;
                }
            }

            if (requestStatus[m_clientFd].getRequestMethod() == "PUT") {
                std::string::size_type beginSize = requestStatus[m_clientFd].recvMsg.size();
                responseStatus[m_clientFd].setBodyFileName(requestStatus[m_clientFd].getRequestResource());
                modifyWaitFd(m_epollFd, m_clientFd, true, true, true);
                requestStatus[m_clientFd].setStatus(HANDLE_BODY);
                std::cout << "[info] client (computing) " << m_clientFd << " Sending a PUT request, the requested resource has been composed into a Response Write event waiting to receive data." << std::endl;
                break;
            }
        }
    }

    if (requestStatus[m_clientFd].getStatus() == HANDLE_COMPLETE) {
        std::cout << "[info] client (computing) " << m_clientFd << " request message was processed successfully" << std::endl;
        requestStatus.erase(m_clientFd);
    } else if (requestStatus[m_clientFd].getStatus() == HANDLE_ERROR) {
        std::cout << "[error] Client " << m_clientFd << " request message processing fails, closing the connection" << std::endl;
        deleteWaitFd(m_epollFd, m_clientFd);
        shutdown(m_clientFd, SHUT_RDWR);
        close(m_clientFd);
        requestStatus.erase(m_clientFd);
    }
}

HandleSend::HandleSend(int clientFd, int epollFd) : m_clientFd(clientFd), m_epollFd(epollFd) {}

void HandleSend::process() {
    std::cout << "[info] Starting client processing " << m_clientFd << " A HandleSend event of the" << std::endl;
    if (responseStatus.find(m_clientFd) == responseStatus.end()) {
        std::cout << "[info] client (computing) " << m_clientFd << " There are no response messages to process" << std::endl;
        return;
    }

    if (responseStatus[m_clientFd].getStatus() == HANDLE_INIT) {
        std::string opera, filename;
        if (responseStatus[m_clientFd].getBodyFileName() == "/") {
            opera = "/";
        } else {
            int i = 1;
            while (i < responseStatus[m_clientFd].getBodyFileName().size() && responseStatus[m_clientFd].getBodyFileName()[i] != '/') {
                ++i;
            }
            if (i < responseStatus[m_clientFd].getBodyFileName().size() - 1) {
                opera = responseStatus[m_clientFd].getBodyFileName().substr(1, i - 1);
                filename = responseStatus[m_clientFd].getBodyFileName().substr(i + 1);
            } else {
                opera = "redirect";
            }
        }

        if (opera == "/") {
            responseStatus[m_clientFd].setBeforeBodyMsg(getStatusLine("HTTP/1.1", "200", "OK"));
            getFileListPage(responseStatus[m_clientFd].getMsgBodyRef());
            responseStatus[m_clientFd].setMsgBodyLen(responseStatus[m_clientFd].getMsgBody().size());
            responseStatus[m_clientFd].setBeforeBodyMsg(responseStatus[m_clientFd].getBeforeBodyMsg() + getMessageHeader(std::to_string(responseStatus[m_clientFd].getMsgBodyLen()), "html"));
            responseStatus[m_clientFd].setBeforeBodyMsg(responseStatus[m_clientFd].getBeforeBodyMsg() + "\r\n");
            responseStatus[m_clientFd].setBeforeBodyMsgLen(responseStatus[m_clientFd].getBeforeBodyMsg().size());
            responseStatus[m_clientFd].setBodyType(HTML_TYPE);
            responseStatus[m_clientFd].setStatus(HANDLE_HEAD);
            responseStatus[m_clientFd].setCurStatusHasSendLen(0);
            std::cout << "[info] client (computing) " << m_clientFd << " The response message is used to return to the file list page, where the status line and message body have been constructed." << std::endl;

        } else if (opera == "downl") {
            responseStatus[m_clientFd].setBeforeBodyMsg(getStatusLine("HTTP/1.1", "200", "OK"));
            responseStatus[m_clientFd].setFileMsgFd(open(("filedir/" + filename).c_str(), O_RDONLY));
            if (responseStatus[m_clientFd].getFileMsgFd() == -1) {
                std::cout << "[error] client (computing) " << m_clientFd << " request message to download the file " << filename << " But the file open failed, exit the current function, re-entry is used to return the redirection message, redirected to the file list" << std::endl;
                responseStatus[m_clientFd] = Response();
                responseStatus[m_clientFd].setBodyFileName("/redirect");
                modifyWaitFd(m_epollFd, m_clientFd, true, true, true);
                return;
            } else {
                struct stat fileStat;
                fstat(responseStatus[m_clientFd].getFileMsgFd(), &fileStat);
                responseStatus[m_clientFd].setMsgBodyLen(fileStat.st_size);
                responseStatus[m_clientFd].setBeforeBodyMsg(responseStatus[m_clientFd].getBeforeBodyMsg() + getMessageHeader(std::to_string(responseStatus[m_clientFd].getMsgBodyLen()), "file", std::to_string(responseStatus[m_clientFd].getMsgBodyLen() - 1)));
                responseStatus[m_clientFd].setBeforeBodyMsg(responseStatus[m_clientFd].getBeforeBodyMsg() + "\r\n");
                responseStatus[m_clientFd].setBeforeBodyMsgLen(responseStatus[m_clientFd].getBeforeBodyMsg().size());
                responseStatus[m_clientFd].setBodyType(FILE_TYPE);
                responseStatus[m_clientFd].setStatus(HANDLE_HEAD);
                responseStatus[m_clientFd].setCurStatusHasSendLen(0);
                std::cout << "[info] client (computing) " << m_clientFd << " request message to download the file " << filename << " File open successful, build response message status line and header information based on file successful" << std::endl;
            }

        } else if (opera == "del") {
            int ret = remove(("filedir/" + filename).c_str());
            if (ret != 0) {
                std::cout << "[error] client (computing) " << m_clientFd << " The request message to delete the file " << filename << " But the file deletion failed" << std::endl;
            } else {
                std::cout << "[info] client (computing) " << m_clientFd << " The request message to delete the file " << filename << " and the file is deleted successfully" << std::endl;
            }

            responseStatus[m_clientFd] = Response();
            responseStatus[m_clientFd].setBodyFileName("/");
            std::cout << "[info] client (computing) " << m_clientFd << " request message is processed, a redirection message is sent" << std::endl;
            modifyWaitFd(m_epollFd, m_clientFd, true, true, true);
            return;

        } else if (opera == "put") {
            std::ofstream ofs("filedir/" + filename, std::ios::out | std::ios::binary);
            if (!ofs) {
                std::cout << "[error] client (computing) " << m_clientFd << " Failed to open file for PUT request " << filename << std::endl;
                responseStatus[m_clientFd] = Response();
                responseStatus[m_clientFd].setBodyFileName("/redirect");
                modifyWaitFd(m_epollFd, m_clientFd, true, true, true);
                return;
            }

            ofs.write(requestStatus[m_clientFd].recvMsg.c_str(), requestStatus[m_clientFd].recvMsg.size());
            ofs.close();

            responseStatus[m_clientFd].setBeforeBodyMsg(getStatusLine("HTTP/1.1", "200", "OK"));
            responseStatus[m_clientFd].setBeforeBodyMsg(responseStatus[m_clientFd].getBeforeBodyMsg() + getMessageHeader("0", "html", "", ""));
            responseStatus[m_clientFd].setBeforeBodyMsg(responseStatus[m_clientFd].getBeforeBodyMsg() + "\r\n");
            responseStatus[m_clientFd].setBeforeBodyMsgLen(responseStatus[m_clientFd].getBeforeBodyMsg().size());
            responseStatus[m_clientFd].setBodyType(EMPTY_TYPE);
            responseStatus[m_clientFd].setStatus(HANDLE_HEAD);
            responseStatus[m_clientFd].setCurStatusHasSendLen(0);

            std::cout << "[info] client (computing) " << m_clientFd << " PUT request processed, response message constructed." << std::endl;

        } else {
            responseStatus[m_clientFd].setBeforeBodyMsg(getStatusLine("HTTP/1.1", "302", "Moved Temporarily"));
            responseStatus[m_clientFd].setBeforeBodyMsg(responseStatus[m_clientFd].getBeforeBodyMsg() + getMessageHeader("0", "html", "/", ""));
            responseStatus[m_clientFd].setBeforeBodyMsg(responseStatus[m_clientFd].getBeforeBodyMsg() + "\r\n");
            responseStatus[m_clientFd].setBeforeBodyMsgLen(responseStatus[m_clientFd].getBeforeBodyMsg().size());
            responseStatus[m_clientFd].setBodyType(EMPTY_TYPE);
            responseStatus[m_clientFd].setStatus(HANDLE_HEAD);
            responseStatus[m_clientFd].setCurStatusHasSendLen(0);
            std::cout << "[info] client (computing) " << m_clientFd << " The response message is a redirected message with the status line and message header constructed" << std::endl;
        }
    }

    while (1) {
        long long sentLen = 0;
        if (responseStatus[m_clientFd].getStatus() == HANDLE_HEAD) {
            sentLen = responseStatus[m_clientFd].getCurStatusHasSendLen();
            sentLen = send(m_clientFd, responseStatus[m_clientFd].getBeforeBodyMsg().c_str() + sentLen, responseStatus[m_clientFd].getBeforeBodyMsgLen() - sentLen, 0);
            if (sentLen == -1) {
                if (errno != EAGAIN) {
                    requestStatus[m_clientFd].setStatus(HANDLE_ERROR);
                    std::cout << "[error] Returned when the response body and message header are sent -1 (errno = " << errno << ")" << std::endl;
                    break;
                }
                break;
            }
            responseStatus[m_clientFd].setCurStatusHasSendLen(responseStatus[m_clientFd].getCurStatusHasSendLen() + sentLen);
            if (responseStatus[m_clientFd].getCurStatusHasSendLen() >= responseStatus[m_clientFd].getBeforeBodyMsgLen()) {
                responseStatus[m_clientFd].setStatus(HANDLE_BODY);
                responseStatus[m_clientFd].setCurStatusHasSendLen(0);
                std::cout << "[info] client (computing) " << m_clientFd << " Response message status line and message header send complete, message body being sent..." << std::endl;
            }

            if (responseStatus[m_clientFd].getBodyType() == FILE_TYPE) {
                std::cout << "[info] client (computing) " << m_clientFd << " The request is for a file, start sending the file " << responseStatus[m_clientFd].getBodyFileName() << " ..." << std::endl;
            }
        }

        if (responseStatus[m_clientFd].getStatus() == HANDLE_BODY) {
            if (responseStatus[m_clientFd].getBodyType() == HTML_TYPE) {
                sentLen = responseStatus[m_clientFd].getCurStatusHasSendLen();
                sentLen = send(m_clientFd, responseStatus[m_clientFd].getMsgBody().c_str() + sentLen, responseStatus[m_clientFd].getMsgBodyLen() - sentLen, 0);
                if (sentLen == -1) {
                    if (errno != EAGAIN) {
                        requestStatus[m_clientFd].setStatus(HANDLE_ERROR);
                        std::cout << "[error] Returned when sending HTML message body -1 (errno = " << errno << ")" << std::endl;
                        break;
                    }
                    break;
                }
                responseStatus[m_clientFd].setCurStatusHasSendLen(responseStatus[m_clientFd].getCurStatusHasSendLen() + sentLen);
                if (responseStatus[m_clientFd].getCurStatusHasSendLen() >= responseStatus[m_clientFd].getMsgBodyLen()) {
                    responseStatus[m_clientFd].setStatus(HANDLE_COMPLETE);
                    responseStatus[m_clientFd].setCurStatusHasSendLen(0);
                    std::cout << "[info] client (computing) " << m_clientFd << " The request was for an HTML file, and the file was sent successfully" << std::endl;
                    break;
                }

            } else if (responseStatus[m_clientFd].getBodyType() == FILE_TYPE) {
                sentLen = responseStatus[m_clientFd].getCurStatusHasSendLen();
                sentLen = sendfile(m_clientFd, responseStatus[m_clientFd].getFileMsgFd(), (off_t *)&sentLen, responseStatus[m_clientFd].getMsgBodyLen() - sentLen);
                if (sentLen == -1) {
                    if (errno != EAGAIN) {
                        requestStatus[m_clientFd].setStatus(HANDLE_ERROR);
                        std::cout << "[error] Returns when sending a file -1 (errno = " << errno << ")" << std::endl;
                        break;
                    }
                    break;
                }
                responseStatus[m_clientFd].setCurStatusHasSendLen(responseStatus[m_clientFd].getCurStatusHasSendLen() + sentLen);
                if (responseStatus[m_clientFd].getCurStatusHasSendLen() >= responseStatus[m_clientFd].getMsgBodyLen()) {
                    responseStatus[m_clientFd].setStatus(HANDLE_COMPLETE);
                    responseStatus[m_clientFd].setCurStatusHasSendLen(0);
                    std::cout << "[info] client (computing) " << m_clientFd << " Requested document delivery completed" << std::endl;
                    break;
                }

            } else if (responseStatus[m_clientFd].getBodyType() == EMPTY_TYPE) {
                responseStatus[m_clientFd].setStatus(HANDLE_COMPLETE);
                responseStatus[m_clientFd].setCurStatusHasSendLen(0);
                std::cout << "[info] client (computing) " << m_clientFd << " The redirected message was sent successfully." << std::endl;
                break;
            }
        }

        if (responseStatus[m_clientFd].getStatus() == HANDLE_ERROR) {
            break;
        }
    }

    if (responseStatus[m_clientFd].getStatus() == HANDLE_COMPLETE) {
        responseStatus.erase(m_clientFd);
        modifyWaitFd(m_epollFd, m_clientFd, true, true, false);
        std::cout << "[info] client (computing) " << m_clientFd << " response message was sent successfully" << std::endl;
    } else if (responseStatus[m_clientFd].getStatus() == HANDLE_ERROR) {
        responseStatus.erase(m_clientFd);
        modifyWaitFd(m_epollFd, m_clientFd, true, false, false);
        shutdown(m_clientFd, SHUT_WR);
        close(m_clientFd);
        std::cout << "[error] client (computing) " << m_clientFd << " The response message to a file descriptor fails, closing the associated file descriptor." << std::endl;
    } else {
        modifyWaitFd(m_epollFd, m_clientFd, true, true, true);
        return;
    }

    if (responseStatus[m_clientFd].getBodyType() == FILE_TYPE) {
        close(responseStatus[m_clientFd].getFileMsgFd());
    }
}

std::string HandleSend::getStatusLine(const std::string &httpVersion, const std::string &statusCode, const std::string &statusDes) {
    std::string statusLine;
    responseStatus[m_clientFd].setResponseHttpVersion(httpVersion);
    responseStatus[m_clientFd].setResponseStatusCode(statusCode);
    responseStatus[m_clientFd].setResponseStatusDes(statusDes);
    statusLine = httpVersion + " " + statusCode + " " + statusDes + "\r\n";
    return statusLine;
}

void HandleSend::getFileListPage(std::string &fileListHtml) {
    std::vector<std::string> fileVec;
    getFileVec("filedir", fileVec);
    
    std::ifstream fileListStream("html/filelist.html", std::ios::in);
    std::string tempLine;
    while (1) {
        getline(fileListStream, tempLine);
        if (tempLine == "<!--filelist_label-->") {
            break;
        }
        fileListHtml += tempLine + "\n";
    }

    for (const auto &filename : fileVec) {
        fileListHtml += "            <tr><td class=\"col1\">" + filename +
                    "</td> <td class=\"col2\"><a href=\"downl/" + filename +
                    "\">downl</a></td> <td class=\"col3\"><a href=\"#\" onclick=\"return confirmDelete('" + filename + "');\">remov</a></td></tr>" + "\n";
    }

    while (getline(fileListStream, tempLine)) {
        fileListHtml += tempLine + "\n";
    }
}

void HandleSend::getFileVec(const std::string &dirName, std::vector<std::string> &resVec) {
    DIR *dir;
    dir = opendir(dirName.c_str());
    struct dirent *stdinfo;
    while (1) {
        stdinfo = readdir(dir);
        if (stdinfo == nullptr) {
            break;
        }
        resVec.push_back(stdinfo->d_name);
        if (resVec.back() == "." || resVec.back() == "..") {
            resVec.pop_back();
        }
    }
}

std::string HandleSend::getMessageHeader(const std::string &contentLength, const std::string &contentType, const std::string &redirectLocation, const std::string &contentRange) {
    std::string headerOpt;

    if (!contentLength.empty()) {
        headerOpt += "Content-Length: " + contentLength + "\r\n";
    }

    if (!contentType.empty()) {
        if (contentType == "html") {
            headerOpt += "Content-Type: text/html;charset=UTF-8\r\n";
        } else if (contentType == "file") {
            headerOpt += "Content-Type: application/octet-stream\r\n";
        }
    }

    if (!redirectLocation.empty()) {
        headerOpt += "Location: " + redirectLocation + "\r\n";
    }

    if (!contentRange.empty()) {
        headerOpt += "Content-Range: 0-" + contentRange + "\r\n";
    }

    headerOpt += "Connection: keep-alive\r\n";

    return headerOpt;
}
