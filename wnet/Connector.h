#pragma once

#include <cerrno>
#include <map>
#include <memory>
#include <mutex>
#include <set>
#include <string>
#include <utility>
#include <sys/socket.h>

#include "Config.h"
#include "Connection.h"
#include "Event.h"
#include "EventPoll.h"
#include "FdCtrl.h"
#include "Log.h"
#include "Noncopyable.h"
#include "ProtoBuf.h"
#include "TimeoutManager.h"

namespace wnet {

class RequestResult : public noncopyable {
  private:
    std::shared_ptr<Connection> masterConnection;
    std::shared_ptr<Connection> subConnection;
    std::shared_ptr<Message> message;
    SubConnectionEventType resultType;
    ParseResult resultDetail;

  public:
    RequestResult(std::shared_ptr<Connection> _masterConnection): masterConnection(_masterConnection),
                                                                  subConnection(nullptr),
                                                                  message(nullptr),
                                                                  resultType(SubConnectionEventType::PENDING),
                                                                  resultDetail(ParseResult::PARSING) {}

    void setSubConnection(std::shared_ptr<Connection> _subConnection) {
      subConnection = _subConnection;
    }

    std::shared_ptr<Connection> getSubConnection() {
      return subConnection;
    }

    bool pending() {
      return resultType == SubConnectionEventType::PENDING;
    }

    bool resolved() {
      return resultType == SubConnectionEventType::RESOLVED;
    }

    bool rejected() {
      return resultType == SubConnectionEventType::REJECTED;
    }

    ParseResult getResultDetail() {
      return resultDetail;
    }

    std::shared_ptr<Message> getData() {
      return message;
    }

    void resolve(std::shared_ptr<Message> _message);

    void reject(ParseResult _resultDetail, std::shared_ptr<Message> _message = nullptr);
};

class ActiveConnectionSet : public noncopyable {
  private:
    std::mutex mtx;
    std::set<std::shared_ptr<Connection>> connectionSet;

  public:
    ActiveConnectionSet() {}
    
    ~ActiveConnectionSet() {}

    std::shared_ptr<Connection> getConnection();

    void removeConnection(std::shared_ptr<Connection> connection);
    
    void insertConnection(std::shared_ptr<Connection> connection);
};

class Connector {
  private:
    static std::mutex mtx;
    // [ [{ip, port} => [ connection,... ] ],... ]
    static std::map< 
      std::pair<std::string, short>, std::shared_ptr<ActiveConnectionSet> 
    > activeConnectionPool;     

    static std::shared_ptr<Connection> getConnection( std::string ip, 
                                                      short port, 
                                                      ConnectionHandler onConnectedHandler = nullptr, 
                                                      ConnectionHandler onReceiveDataHandler = nullptr, 
                                                      ConnectionHandler onDisconnectingHandler = nullptr);

    static std::shared_ptr<ActiveConnectionSet> buildNewPool(std::string ip, short port);

    static std::shared_ptr<ActiveConnectionSet> getConnectionPool(std::string ip, short port);

    static std::shared_ptr<Connection> connectTo( std::string ip, 
                                                  short port, 
                                                  ConnectionHandler onConnectedHandler = nullptr, 
                                                  ConnectionHandler onReceiveDataHandler = nullptr, 
                                                  ConnectionHandler onDisconnectingHandler = nullptr);

  public:

    static std::shared_ptr<RequestResult> initSubRequest( std::string ip, 
                                                          short port, 
                                                          std::shared_ptr<Message> requestData, 
                                                          std::shared_ptr<Connection> masterConnection,
                                                          int timeoutSeconds = SUB_REQUEST_TIMEOUT_SECONDS);

    static void removeFromConnectionPool(std::shared_ptr<Connection> connection);

    static void insertIntoConnectionPool(std::shared_ptr<Connection> connection);
    
};

}