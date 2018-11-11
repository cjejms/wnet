#pragma once

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <functional>
#include <memory>
#include <mutex>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <unistd.h>

#include "Config.h"
#include "Connection.h"
#include "Log.h"
#include "Noncopyable.h"

namespace wnet {

class EventPoll; 

class TCPServer : public noncopyable, public std::enable_shared_from_this<TCPServer> {
  private:
    const short port;
    int server_fd;
    std::shared_ptr<EventPoll> eventPoll;
    ConnectionHandler onConnectedHandler,
                      onReceiveDataHandler,
                      onDisconnectingHandler;
    
    struct sockaddr_in serverAddr, clientAddr;
    
    bool running = false;

    bool enableConnectionKeepAlive = false;

    void bindPort();

  public:
    TCPServer(short _port): port(_port), 
                            onConnectedHandler(nullptr), 
                            onReceiveDataHandler(nullptr), 
                            onDisconnectingHandler(nullptr) {}
    ~TCPServer() {
      LOG(LogLevel::DEBUG, "[TCPServer] destructing");
      ::close(server_fd);
    }

    void setEnableConnectionKeepAlive();
    
    void setOnConnectedHandler(ConnectionHandler handler);

    void setOnReceiveDataHandler(ConnectionHandler handler);

    void setOnDisconnectingHandler(ConnectionHandler handler);

    void run();

    void handleNewConnection();

    void shutdown();

};

}