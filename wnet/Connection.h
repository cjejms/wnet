#pragma once

#include <cerrno>
#include <cstring>
#include <memory>
#include <string>
#include <vector>
#include <unistd.h>
#include <sys/socket.h>

#include "Buffer.h"
#include "Event.h"
#include "Log.h"
#include "Noncopyable.h"
#include "ProtoBuf.h"

namespace wnet {

class EventPoll;
class RequestResult;
class TimeoutEntry;

enum class ConnectionType {
  ACTIVE = 1, 
  PASSIVE
};

enum class ConnectionStatus { 
  CONNECTING = 1, 
  CONNECTED, 
  DISCONNECTING, 
  DISCONNECTED 
};

using ConnectionHandler = std::function<void(Connection* const)>;

class Connection : public noncopyable, public std::enable_shared_from_this<Connection> {
  private:
    const int fd;
    std::string serverIP = "";
    short serverPort = 0;
    std::string clientIP = "";

    std::shared_ptr<EventPoll> eventPoll;

    std::weak_ptr<TimeoutEntry> timeoutEntry;

    ConnectionStatus status;

    ConnectionType type;

    std::shared_ptr<Buffer> inputBuf,
                            outputBuf;
    bool readable = false;
    bool writable = false;

    ConnectionHandler onConnectedHandler, 
                      onReceiveDataHandler, 
                      onDisconnectingHandler,
                      subConnectionCallBackHandler; 

    int requestIDForTimeout = 0;    // record ID for sub request

    void receiveData();

    void sendData();

    void setSubConnectionCallBackHandler(ConnectionHandler handler) {
      subConnectionCallBackHandler = handler;
    }
    
  public:
    Connection( int _fd, 
                std::shared_ptr<EventPoll> _eventPoll, 
                ConnectionType _type = ConnectionType::PASSIVE,
                ConnectionHandler _onConnectedHandler = nullptr, 
                ConnectionHandler _onReceiveDataHandler = nullptr, 
                ConnectionHandler _onDisconnectingHandler = nullptr): fd(_fd), 
                                                              eventPoll(_eventPoll),
                                                              status(ConnectionStatus::CONNECTING),
                                                              type(_type),
                                                              onConnectedHandler(_onConnectedHandler), 
                                                              onReceiveDataHandler(_onReceiveDataHandler), 
                                                              onDisconnectingHandler(_onDisconnectingHandler) {
      inputBuf = std::make_shared<Buffer>();
      outputBuf = std::make_shared<Buffer>();
    }

    std::shared_ptr<Connection> thisConnection() {
      return shared_from_this();
    }

    ~Connection() {
      LOG(LogLevel::DEBUG, "[Connection][fd %d] destructed", fd);
      if(status < ConnectionStatus::DISCONNECTED) {
        terminate();
      }
    }

    int get_fd() {
      return fd;
    }

    void setServerInfo(std::string _serverIP, short _serverPort) {
      serverIP = _serverIP;
      serverPort = _serverPort;
    }

    std::string getServerIP() {
      return serverIP;
    }

    short getServerPort() {
      return serverPort;
    }

    void setClientInfo(std::string _clientIP) {
      clientIP = _clientIP;
    }

    std::string getClientIP() {
      return clientIP;
    }

    std::shared_ptr<TimeoutEntry> setTimeoutEntry();

    std::shared_ptr<TimeoutEntry> getTimeoutEntry();

    std::shared_ptr<Buffer> getInputBuffer() {
      return inputBuf;
    }

    std::shared_ptr<Buffer> getOutputBuffer() {
      return outputBuf;
    }

    void setOnConnectedHandler(ConnectionHandler handler) {
      onConnectedHandler = handler;
    }

    void setOnReceiveDataHandler(ConnectionHandler handler) {
      onReceiveDataHandler = handler;
    }

    void setOnDisconnectingHandler(ConnectionHandler handler) {
      onDisconnectingHandler = handler;
    }

    void await(std::vector<std::shared_ptr<RequestResult>> requestResultList, ConnectionHandler handler);

    // only for subconnection
    void resolve(std::shared_ptr<Connection> masterConnection);

    // only for subconnection
    void reject(std::shared_ptr<Connection> masterConnection);

    void requestResolved();
    
    // handle connection assigned to this connection, called by EventLoop in working threads
    void handleEvent(std::shared_ptr<Event> event);
    
    void setTimeout(int seconds, std::function<void()> timeoutHandler);

    // info TimeoutManager this connection is still working 
    // only works when constructed with idle timeout initiated
    void clockIn();

    // issue events to other onnection or whatever
    void issueEvent(std::shared_ptr<Event> event);

    template<typename TYPE>
    void writeData(TYPE data) { // you can write any data supported by Buffer
      if(isConnected()) {
        clockIn();
        outputBuf->append(data);
      } else {
        LOG(LogLevel::DEBUG, "[Connection][fd %d][status %d] connection not connected, unable to write data", fd, status);
      }
    }

    // decode protobuf message from input buffer
    std::shared_ptr<Message> decodeMessage(ParseResult& parseResult);

    // can be used for simulating level triggered read events
    void issueIOEventToSelf(IOEventType event);

    bool isConnected() {
      return status == ConnectionStatus::CONNECTED;
    }

    bool isDisconnecting() {
      return status == ConnectionStatus::DISCONNECTING;
    }

    // for recycling active connection
    void reInit(ConnectionHandler _onConnectedHandler = nullptr, 
                ConnectionHandler _onReceiveDataHandler = nullptr, 
                ConnectionHandler _onDisconnectingHandler = nullptr);

    // gracefully, just mark this connection ConnectionStatus::DISCONNECTING, 
    // data remaining in outputBuf will still be sent
    void shutdown();

    // actually kill this connection, mustn't call in user code
    void terminate();   

};

}