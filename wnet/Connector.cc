#include "Connector.h"

using namespace wnet;

// for class RequestResult
void RequestResult::resolve(std::shared_ptr<Message> _message) {
  if(pending()) {
    message = _message;
    resultType = RESOLVED;
    resultDetail = PARSE_SUCCESS;
    subConnection->resolve(masterConnection);
    Connector::insertIntoConnectionPool(subConnection);
    LOG(DEBUG, "[RequestResult][subConnection][fd %d] subConnection RESOLVED", subConnection->get_fd());
  }
  
}

void RequestResult::reject(ParseResult _resultDetail, std::shared_ptr<Message> _message) {
  if(pending()) {
    message = _message;
    resultType = REJECTED;
    resultDetail = _resultDetail;
    subConnection->reject(masterConnection);
    LOG(DEBUG, "[RequestResult][subConnection][fd %d] subConnection REJECTED", subConnection->get_fd());
  }
}


// for class ActiveConnectionSet
std::shared_ptr<Connection> ActiveConnectionSet::getConnection() {
  if(connectionSet.empty()) {
    return nullptr;
  }
  std::lock_guard<std::mutex> guard(mtx);
  while(!connectionSet.empty()) {
    auto iterator = connectionSet.begin();
    connectionSet.erase(iterator);
    if((*iterator)->isConnected()) {
      return *iterator;
    }
  }
  return nullptr;
}

void ActiveConnectionSet::removeConnection(std::shared_ptr<Connection> connection) {
  std::lock_guard<std::mutex> guard(mtx);
  auto iterator = connectionSet.find(connection);
  if(iterator != connectionSet.end()) {
    connectionSet.erase(iterator);
  }
}

void ActiveConnectionSet::insertConnection(std::shared_ptr<Connection> connection) {
  std::lock_guard<std::mutex> guard(mtx);
  connectionSet.insert(connection);
}


// for class Connector
std::mutex Connector::mtx;
std::map< std::pair<std::string, short>, std::shared_ptr<ActiveConnectionSet> > Connector::activeConnectionPool; 

std::shared_ptr<Connection> Connector::getConnection( std::string ip, 
                                                      short port, 
                                                      ConnectionHandler onConnectedHandler, 
                                                      ConnectionHandler onReceiveDataHandler, 
                                                      ConnectionHandler onDisconnectingHandler) {

  if(auto connection = getConnectionPool(ip, port)->getConnection()) {
    // found corresponding ActiveConnectionSet and fetch connection from pool
    connection->reInit(onConnectedHandler, onReceiveDataHandler, onDisconnectingHandler);
    return connection;
  } else {
    return connectTo(ip, port, onConnectedHandler, onReceiveDataHandler, onDisconnectingHandler);
  }
}

std::shared_ptr<ActiveConnectionSet> Connector::buildNewPool(std::string ip, short port) {
    std::lock_guard<std::mutex> guard(mtx);
    if(activeConnectionPool.find(std::pair<std::string,short>(ip, port)) != activeConnectionPool.end()) {
      return getConnectionPool(ip, port);
    } else {
      auto connectionSet = std::make_shared<ActiveConnectionSet>();
      activeConnectionPool[std::pair<std::string,short>(ip, port)] = connectionSet;
      return connectionSet;
    }
}

std::shared_ptr<ActiveConnectionSet> Connector::getConnectionPool(std::string ip, short port) {
  auto iterator = activeConnectionPool.find(std::pair<std::string,short>(ip, port));
  if(iterator != activeConnectionPool.end()) {
    return (*iterator).second;
  } else {
    // have to build a new one 
    return buildNewPool(ip, port);
  }
}

std::shared_ptr<Connection> Connector::connectTo( std::string ip, 
                                                  short port, 
                                                  ConnectionHandler onConnectedHandler, 
                                                  ConnectionHandler onReceiveDataHandler, 
                                                  ConnectionHandler onDisconnectingHandler) {
  int client_fd = ::socket(AF_INET, SOCK_STREAM, 0);
  FdCtrl::setNoneBlock(client_fd);
  FdCtrl::addFdFlag(client_fd, FD_CLOEXEC);
  
  struct sockaddr_in serverAddr;
  ::memset(&serverAddr, 0, sizeof serverAddr);
  serverAddr.sin_family = AF_INET;
  serverAddr.sin_addr.s_addr = ::inet_addr(ip.c_str()); 
  serverAddr.sin_port = ::htons(port);

  int result = ::connect(client_fd, reinterpret_cast<struct sockaddr*>(&serverAddr), sizeof serverAddr);
  
  LOG(DEBUG, "[Connector][fd %d][connect()] return: %d error: [%d]%s", client_fd, result, errno, ::strerror(errno));
  if(result == -1 && errno == EINPROGRESS) {
    LOG(INFO, "[Connector][fd %d] new active connection established", client_fd);
  } else {
    return nullptr;
  }

  auto eventPoll = EventPoll::getInstance();
  // construct connection object and register to eventpoll connection set
  auto connection = std::make_shared<Connection>( client_fd, 
                                                  eventPoll, 
                                                  ACTIVE, 
                                                  onConnectedHandler,
                                                  onReceiveDataHandler,
                                                  onDisconnectingHandler );
  connection->setServerInfo(ip, port);
  connection->setClientInfo("127.0.0.1");

  eventPoll->addConnection(connection);
  eventPoll->addEventListener(client_fd, EventPoll::READ_EVENT | EventPoll::WRITE_EVENT); 

  return connection;
}

std::shared_ptr<RequestResult> Connector::initSubRequest( std::string ip, 
                                                          short port, 
                                                          std::shared_ptr<Message> requestData, 
                                                          std::shared_ptr<Connection> masterConnection,
                                                          int timeoutSeconds) {
  auto requestResult = std::make_shared<RequestResult>(masterConnection);
  auto _subConnection = getConnection(
    // connect to ip:port
    ip, port, 
    ////////////////////////
    // onConnectedHandler	//
    ////////////////////////
    [=](Connection* const subConnection) {		
      // send request data on connection established
      subConnection->writeData(requestData);
      // reject this request and shut down subconnection on timeout
      subConnection->setTimeout(timeoutSeconds, [=]{
        if(requestResult->pending()) {
          requestResult->reject(TIMEOUT);
          subConnection->terminate();
        }
      });
    }, 
    //////////////////////////
    // OnReceiveDataHandler	//
    //////////////////////////
    [=](Connection* const subConnection) {
      ParseResult parseResult;
      std::shared_ptr<Message> responseData = subConnection->decodeMessage(parseResult);
      if(parseResult == PARSE_SUCCESS) {
        // store the reponse data and generate SubConnectionEvent to notify master connection
        requestResult->resolve(responseData);
      } else {
        if(parseResult != MESSAGE_INCOMPLETED) {
          requestResult->reject(parseResult);
        }
      }
    }, 
    ////////////////////////////
    // onDisconnectingHandler	//
    ////////////////////////////
    [=](Connection* const subConnection) {
      requestResult->reject(CONNECTION_CLOSED_BY_PEER);
    }
  );
  requestResult->setSubConnection(_subConnection);

  return requestResult;
}

void Connector::removeFromConnectionPool(std::shared_ptr<Connection> connection) {
  getConnectionPool(connection->getServerIP(), connection->getServerPort())->removeConnection(connection);
}

void Connector::insertIntoConnectionPool(std::shared_ptr<Connection> connection) {
  getConnectionPool(connection->getServerIP(), connection->getServerPort())->insertConnection(connection);
}