#include "Connection.h"
#include "TimeoutManager.h"

using namespace wnet;

TimeoutEntry::~TimeoutEntry() {
  if(timeoutHandler) {
    // self-defined timeout behaviour
    timeoutHandler();
  } else {
    // connection lifetime timeout control (terminate if idle for a while)
    auto connectionPtr = getConnection();
    if(connectionPtr) {
      connectionPtr->terminate(); 
    }
  }
}

std::shared_ptr<Connection> TimeoutEntry::getConnection() {
  auto connectionPtr = connection.lock();
  if (!connectionPtr) {
    LOG(LogLevel::DEBUG, "[TimeoutEntry] connection already perished");
    return nullptr;
  } else {
    return connectionPtr;
  }
}