#include "Connection.h"
#include "Connector.h"
#include "EventPoll.h"
#include "TimeoutManager.h"

using namespace wnet;

void Connection::receiveData() {
  while((isConnected() || isDisconnecting()) && readable) {
    if(inputBuf->space() == 0) {
      inputBuf->allocate();
    }
    ssize_t readLen = ::read(fd, inputBuf->end(), inputBuf->space());

    if(readLen > 0) {     // read normally
      if(isConnected()) {
        // when ConnectionStatus::DISCONNECTING, input data is ignored
        inputBuf->addSize(readLen);
        LOG(LogLevel::DEBUG, "[Connection][fd %d] read %d bytes", fd, static_cast<int>(readLen));
      }
      continue;
    }
    if(readLen == 0) {    
      LOG(LogLevel::INFO, "[Connection][fd %d] connection closed by peer", fd);
      terminate();    // closed by peer, just terminate this connection 
      return;
    }
    if(readLen == -1) {
      if(errno == EINTR) {  // interrupted by signals, just ignore
        continue;
      }
      if(errno == EAGAIN || errno == EWOULDBLOCK) {  // blocked, no more data available for now
        readable = false;
        return;
      }
      LOG(LogLevel::DEBUG, "[Connection][fd %d][read() -1] error: [%d]%s", fd, errno, ::strerror(errno));
    }
    terminate();
    return;
  }
}

void Connection::sendData() {
  while((isConnected() || isDisconnecting()) && writable && !outputBuf->empty()) {

    if(outputBuf->size() == 0) {
      outputBuf->allocate();
    }
    ssize_t writeLen = ::write(fd, outputBuf->begin(), outputBuf->size());
    
    if(writeLen > 0) {
      LOG(LogLevel::DEBUG, "[Connection][fd %d] write %d bytes", fd, static_cast<int>(writeLen));
      outputBuf->consume(writeLen);
      continue;
    }

    if(writeLen == -1) {
      if(errno == EINTR) {  // interrupted by signals, just ignore
        continue;
      }
      if(errno == EAGAIN || errno == EWOULDBLOCK) {  // blocked, system socket send buffer is full
        writable = false;
        return;
      }
      LOG(LogLevel::DEBUG, "[Connection][fd %d][write() -1] error: [%d]%s", fd, errno, ::strerror(errno));
      terminate();
    }
    return;
  }
}

std::shared_ptr<TimeoutEntry> Connection::setTimeoutEntry() {
  auto _timeoutEntry = std::make_shared<TimeoutEntry>(shared_from_this());
  timeoutEntry = _timeoutEntry;
  return _timeoutEntry;
}

std::shared_ptr<TimeoutEntry> Connection::getTimeoutEntry() {
  if(auto timeoutEntryPtr = timeoutEntry.lock()) {
    return timeoutEntryPtr;
  } else {
    return nullptr;
  }
}

void Connection::await(std::vector<std::shared_ptr<RequestResult>> requestResultList, ConnectionHandler handler) {
  // check if any subRequestResult still pending
  for(auto subRequestResult : requestResultList) {
    if(subRequestResult->pending()) {
      // then await SubConnectionEvent
      setSubConnectionCallBackHandler([=](Connection* const masterConnection) {
        for(auto requestResult : requestResultList) {
          if(requestResult->pending()) {
            return;
          }
        }
        handler(masterConnection);
      });
      return;
    }
  }
  // none subRequestResult is pending, good to go, SubConnectionEvent won't disturb following request
  handler(this);
}

void Connection::resolve(std::shared_ptr<Connection> masterConnection) {
  EventDetail detail;
  detail.subConnectionEvent = new SubConnectionEvent(
    masterConnection, 
    shared_from_this(), 
    SubConnectionEventType::RESOLVED
  );
  issueEvent(std::make_shared<Event>(EventType::SUBCONNECTION_EVENT, detail));
  // clear handlers
  onConnectedHandler = nullptr;
  onReceiveDataHandler = nullptr;
  onDisconnectingHandler = nullptr;
}

void Connection::reject(std::shared_ptr<Connection> masterConnection) {
  EventDetail detail;
  detail.subConnectionEvent = new SubConnectionEvent(
    masterConnection, 
    shared_from_this(), 
    SubConnectionEventType::REJECTED
  );
  issueEvent(std::make_shared<Event>(EventType::SUBCONNECTION_EVENT, detail));
  terminate();
}

void Connection::requestResolved() {
  subConnectionCallBackHandler = nullptr;
  if(!inputBuf->empty()) {
    issueIOEventToSelf(IOEventType::READ_EVENT);
  }
}

void Connection::handleEvent(std::shared_ptr<Event> event) {
  switch(event->getType()) {
    case EventType::IO_EVENT:
      {
        // update readable & writable status according to IOEvent
        IOEvent* activeIoEvent = event->getEventDetail().ioEvent;
        switch(activeIoEvent->getEvent()) {
          case IOEventType::READ_EVENT:
            readable = true;
            break;

          case IOEventType::WRITE_EVENT:
            writable = true;
            break;
            
          case IOEventType::READ_AND_WRITE_EVENT:
            readable = true;
            writable = true;
            break;
        }
      }
      break;

    case EventType::SUBCONNECTION_EVENT:
      {
        SubConnectionEvent* activeSubConnectionEvent = event->getEventDetail().subConnectionEvent;
        if(isConnected() && subConnectionCallBackHandler) {
          LOG(LogLevel::DEBUG, "[Connection][fd %d] SUBCONNECTION_EVENT activated, subconnection[fd %d]", fd, activeSubConnectionEvent->getSubConnection()->get_fd());
          subConnectionCallBackHandler(this);
        }
      }
      break;

    default:
      LOG(LogLevel::ERROR, "[Connection] receiving unsupported event");
      shutdown();
      break;
  }
  LOG(LogLevel::DEBUG, "[Connection][fd %d][status %d][ readable %d][writable %d]", fd, status, readable, writable);
  
  // call on connected handler 
  if(status == ConnectionStatus::CONNECTING) {
    status = ConnectionStatus::CONNECTED;
    if(onConnectedHandler) {
      LOG(LogLevel::DEBUG, "[Connection][fd %d] call onConnectedHandler", fd);
      onConnectedHandler(this);
    }
  }

  sendData();
  receiveData();

  if(isConnected()) {
    if(inputBuf->size() > 0 && onReceiveDataHandler && !subConnectionCallBackHandler) {
      // data remain to handle, call onReceiveDataHandler 
      LOG(LogLevel::DEBUG, "[Connection][fd %d] call onReceiveDataHandler", fd);
      onReceiveDataHandler(this);
    }
  }
  
  sendData();
  
  // simulate level trigger mode readable event (EPOLLLT | EPOLLIN)
  if(isConnected() && readable) {  
    issueIOEventToSelf(IOEventType::READ_EVENT);
  }

  if(isDisconnecting() && outputBuf->empty()) {  
    // connection ConnectionStatus::DISCONNECTING and all data have been sent 
    // send FIN to peer
    ::shutdown(fd, SHUT_WR);
  }
}

void Connection::setTimeout(int seconds, std::function<void()> timeoutHandler) {
  eventPoll->setTimeout(seconds, shared_from_this(), timeoutHandler);
}

void Connection::clockIn() {
  if(getTimeoutEntry()) {
    eventPoll->connectionClockIn(thisConnection());
  } else {
    LOG(LogLevel::DEBUG, "[Connection][fd %d] this connection doesn't support IDLE timeout", fd);
  }
  
}

void Connection::issueEvent(std::shared_ptr<Event> event) {
  eventPoll->eventDispatcher(event);
}

std::shared_ptr<Message> Connection::decodeMessage(ParseResult& parseResult) {
  return inputBuf->fetchMessage(parseResult);
}

void Connection::issueIOEventToSelf(IOEventType event) {
  // std::this_thread::sleep_for(std::chrono::seconds(1));   // for debug, will be removed
  LOG(LogLevel::DEBUG, "[Connection][fd %d][event %d] going back to queue, will process later", fd, event);
  
  EventDetail detail;
  detail.ioEvent = new IOEvent(fd, event, shared_from_this());
  issueEvent(std::make_shared<Event>(EventType::IO_EVENT, detail));
}

void Connection::reInit(ConnectionHandler _onConnectedHandler, 
                        ConnectionHandler _onReceiveDataHandler, 
                        ConnectionHandler _onDisconnectingHandler) {
  status = ConnectionStatus::CONNECTING;
  onConnectedHandler = _onConnectedHandler;
  onReceiveDataHandler = _onReceiveDataHandler;
  onDisconnectingHandler = _onDisconnectingHandler;
  // to trigger _onConnectedHandler and send request data
  issueIOEventToSelf(IOEventType::WRITE_EVENT);
}

// shut down this connection gracefully
void Connection::shutdown() {
  if(status < ConnectionStatus::DISCONNECTING) {
    // https://stackoverflow.com/questions/8874021/close-socket-directly-after-send-unsafe
    
    // send remaining data in output buffer,
    // then send a FIN to peer, and expecting receiving a FIN back
    status = ConnectionStatus::DISCONNECTING;
    LOG(LogLevel::INFO, "[Connection][fd %d] shutting down gracefully", fd);
  }
}

void Connection::terminate() {
  if(status < ConnectionStatus::DISCONNECTED) {
    status = ConnectionStatus::DISCONNECTED;

    if(type == ConnectionType::ACTIVE) {
      Connector::removeFromConnectionPool(shared_from_this());
    }

    if(onDisconnectingHandler) {
      // do some cleaning work in this handler
      onDisconnectingHandler(this);
    }
    
    eventPoll->removeEventListener(fd);
    ::close(fd);

    // no new io event will come, so it's safe to remove this connection from connection set
    eventPoll->removeConnection(fd);

    // this connection is actually disconnected, 
    // and this connection instance will just remain  
    // until last shared_ptr pointing to it is destructed
  }
}

