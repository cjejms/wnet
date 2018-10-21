#include "EventPoll.h"
#include "FdCtrl.h"
#include "TCPServer.h"

using namespace wnet;

void TCPServer::bindPort() {
  bzero(&serverAddr, sizeof serverAddr);
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_addr.s_addr = INADDR_ANY;
  serverAddr.sin_port = htons(port);     // Host to Network Short
  if(::bind(server_fd, reinterpret_cast<struct sockaddr*>(&serverAddr), sizeof serverAddr) == -1) {
    LOG(FATAL, "[TCPServer][fd %d][bind()] bind to port: %d failed, error: [%d]%s", server_fd, port, errno, ::strerror(errno));
    ::exit(EXIT_FAILURE);
  }
  FdCtrl::setReuseAddr(server_fd);
  FdCtrl::setReusePort(server_fd);
  FdCtrl::addFdFlag(server_fd, FD_CLOEXEC);
}

void TCPServer::setEnableConnectionKeepAlive() {
  enableConnectionKeepAlive = true;
}

void TCPServer::setOnConnectedHandler(ConnectionHandler handler) {
  if(running) {
    LOG(ERROR, "[TCPServer] server running, register onConnectedHandler failed");
  } else {
    onConnectedHandler = handler;
  }
}

void TCPServer::setOnReceiveDataHandler(ConnectionHandler handler) {
  if(running) {
    LOG(ERROR, "[TCPServer] server running, register onReceiveDataHandler failed");
  } else {
    onReceiveDataHandler = handler;
  }
}

void TCPServer::setOnDisconnectingHandler(ConnectionHandler handler) {
  if(running) {
    LOG(ERROR, "[TCPServer] server running, register onDisconnectingHandler failed");
  } else {
    onDisconnectingHandler = handler;
  }
}

void TCPServer::run() {
  server_fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if(server_fd == -1) {
    LOG(FATAL, "[TCPServer][socket()] socket fd generate failed, error: [%d]%s", errno, ::strerror(errno));
    ::exit(EXIT_FAILURE);
  }
  FdCtrl::setNoneBlock(server_fd);
  
  eventPoll = EventPoll::getInstance();
  eventPoll->setServer(server_fd, shared_from_this());
  eventPoll->addEventListener(server_fd, EventPoll::READ_EVENT, EventPoll::LEVEL_TRIGGER);   // server fd LT trigger

  bindPort();
  if(::listen(server_fd, SERVER_LISTEN_QUEUE_LENGTH) == -1) {
    LOG(FATAL, "[TCPServer][fd %d][listen()] socket start listen failed, error: [%d]%s", server_fd, errno, ::strerror(errno));
    ::exit(EXIT_FAILURE);
  }
  LOG(INFO, "[TCPServer][port %d][fd %d] server start running", port, server_fd);
  running = true;
  while(running) {
    eventPoll->poll();
  }
  eventPoll->shutdown();
  LOG(INFO, "[TCPServer] server shut down");
}

void TCPServer::handleNewConnection() {
  if(running) {
    bzero(&clientAddr, sizeof clientAddr);
    socklen_t clilen = sizeof clientAddr;
    int connection_fd = ::accept(server_fd, reinterpret_cast<struct sockaddr*>(&clientAddr), &clilen);
    if(connection_fd == -1) {
      LOG(FATAL, "[TCPServer][fd %d][accept()] handle new connection failed, error: [%d]%s", server_fd, errno, ::strerror(errno));
      ::exit(EXIT_FAILURE);
    }
    FdCtrl::setNoneBlock(connection_fd);

    char *clientIP = ::inet_ntoa(clientAddr.sin_addr);
    LOG(INFO, "[TCPServer] accept new connection from %s", clientIP);

    // construct connection object and register to eventpoll connection set
    auto connection = std::make_shared<Connection>( connection_fd, 
                                                    eventPoll, 
                                                    PASSIVE, 
                                                    onConnectedHandler,
                                                    onReceiveDataHandler,
                                                    onDisconnectingHandler );
    connection->setServerInfo("127.0.0.1", port);
    connection->setClientInfo(clientIP);
    
    eventPoll->addConnection(connection);
    if(enableConnectionKeepAlive) {
      eventPoll->connectionIniIdleTimeout(connection);      // kill the connection if idle for some time
    }
 
    eventPoll->addEventListener(connection_fd, EventPoll::READ_EVENT | EventPoll::WRITE_EVENT);
  }
}

void TCPServer::shutdown() {
  running = false;
}


