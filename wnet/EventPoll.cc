#include "Connection.h"
#include "Event.h"
#include "EventLoop.h"
#include "EventPoll.h"
#include "EventQueue.h"
#include "TCPServer.h"
#include "TimeoutManager.h"
#include "Timer.h"

using namespace wnet;

EventPoll::EventPoll(): running(true) {
  // init EventPoll
  epoll_fd = ::epoll_create1(EPOLL_CLOEXEC);
  if(epoll_fd == -1) {
    LOG(FATAL, "[EventPoll][epoll_create1()] EventPoll initialize failed, error: [%d]%s", errno, ::strerror(errno));
    ::exit(EXIT_FAILURE);
  }

  // init Timer
  timer = std::make_shared<Timer>();
  timer_fd = timer->get_fd();
  addEventListener(timer_fd, EventPoll::READ_EVENT);
  timer->startTickTock();

  for(int index = 0; index < EVENT_LOOP_COUNT; index++) {
    // init EventQueue
    auto eventQueue = std::make_shared<EventQueue>(index);
    eventQueueList.push_back(eventQueue);

    // init EventLoop
    auto eventLoop = std::make_shared<EventLoop>(index, eventQueue);
    eventLoopList.push_back(eventLoop);

    // init EventLoop thread
    eventLoopThreadList.push_back(
      std::thread([=] {
        eventLoop->loop();
      })
    );
  }
}

std::shared_ptr<EventLoop> EventPoll::getEventLoop(std::shared_ptr<Connection> connection) {
  return eventLoopList[connection->get_fd() % EVENT_LOOP_COUNT];
}

std::shared_ptr<EventQueue> EventPoll::getEventQueue(std::shared_ptr<Connection> connection) {
  return eventQueueList[connection->get_fd() % EVENT_LOOP_COUNT];
}

void EventPoll::broadcastEvent(std::shared_ptr<Event> event) {
  for(auto eventQueue : eventQueueList) {   // broadcast this event to all EventLoop
    eventQueue->addEvent(event);
  }
}

std::shared_ptr<EventPoll> EventPoll::thisPtr = nullptr;
std::mutex EventPoll::mutx;
// different threads share one file descriptor set
// so only one EvevtPoll instance every process
std::shared_ptr<EventPoll> EventPoll::getInstance() { 
  if(thisPtr == nullptr) {
    std::lock_guard<std::mutex> guard(mutx);
    if(thisPtr == nullptr) {
      thisPtr = std::shared_ptr<EventPoll>(new EventPoll());
    }
  }
  return thisPtr;
}

void EventPoll::setServer(int _server_fd, std::shared_ptr<TCPServer> _server) {
  if(server_fd >= 0) {
    LOG(FATAL, "[EventPoll] set server failed, cannot set multiple servers");
    ::exit(EXIT_FAILURE);
  }
  server_fd = _server_fd;
  server = _server;
}

void EventPoll::addConnection(std::shared_ptr<Connection> connection) {
  connectionSet[connection->get_fd()] = connection;
  LOG(INFO, "[EventPoll] holding %d connections", static_cast<int>(connectionSet.size()));
}

std::shared_ptr<Connection> EventPoll::getConnection(int connection_fd) {
  if(running && connectionSet.find(connection_fd) != connectionSet.end()) {
    return connectionSet[connection_fd];
  } else {
    return nullptr;
  }
}

void EventPoll::removeConnection(int connection_fd) {
  // must only call after closing connection fd or reomved from epoll
  auto iterator = connectionSet.find(connection_fd);
  if(iterator != connectionSet.end()) {
    connectionSet.erase(iterator);
  } 
}

void EventPoll::connectionIniIdleTimeout(std::shared_ptr<Connection> connection) {
  if(running) {
    // connection will automaticly shutdown after certain length of idle time
    getEventLoop(connection)->getTimeoutManager()->clockIn(connection->setTimeoutEntry());
  }
}

void EventPoll::connectionClockIn(std::shared_ptr<Connection> connection) {
  if(running) {
    // works only when connection constructed with idle timeout initiated ( connectionIniIdleTimeout() )
    getEventLoop(connection)->getTimeoutManager()->clockIn(connection->getTimeoutEntry());
  }
}

void EventPoll::setTimeout( int seconds, 
                            std::shared_ptr<Connection> connection, 
                            std::function<void()> timeoutHandler) {
  getEventLoop(connection)->getTimeoutManager()->setTimeout(seconds, connection, timeoutHandler);
}

void EventPoll::poll() {
  int activeEventCount = ::epoll_wait(epoll_fd, readyEvents, MAX_READY_EVENT_PER_POLL, EPOLL_WAIT_TIMEOUT);
  EventDetail detail;

  for(int i = 0; i < activeEventCount && running; ++i) {
    int activeEvent_fd = readyEvents[i].data.fd;
    int activeEvent = readyEvents[i].events;

    if(activeEvent_fd == timer_fd) {
      // timeout event triggered, dispatch to every eventloop
      detail.timeoutEvent = new TimeoutEvent(timer->handle());
      eventDispatcher(std::make_shared<Event>(TIMEOUT_EVENT, detail));
      
    } else if(server_fd >= 0 && activeEvent_fd == server_fd) {
      //////////////////////////////////
      ///  new connection call back  ///
      //////////////////////////////////
      auto serverPtr = server.lock();
      if (!serverPtr) {
        LOG(FATAL, "[EventPoll] server ptr lost, unable to handle new connection, terminated");
        ::exit(EXIT_FAILURE);
      }
      serverPtr->handleNewConnection();
      
    } else if((activeEvent & READ_EVENT) || (activeEvent & WRITE_EVENT)) {
      LOG(DEBUG, "[EventPoll] IO_EVENT actived, passing to eventloop");
      if(auto connection = getConnection(activeEvent_fd)) {
        IOEventType type;
        if(activeEvent & READ_EVENT) {
          type = ::READ_EVENT;
        }
        if(activeEvent & WRITE_EVENT) {
          type = ::WRITE_EVENT;
        }
        if((activeEvent & READ_EVENT) && (activeEvent & WRITE_EVENT)) {
          type = ::READ_AND_WRITE_EVENT;
        }
        
        detail.ioEvent = new IOEvent(activeEvent_fd, type, connection);
        eventDispatcher(std::make_shared<Event>(IO_EVENT, detail));
        
      } else {
        LOG(ERROR, "[EventPoll] unable to get connection, probably disconnecting, fd: %d, event: %d", activeEvent_fd, activeEvent);
      }
    
    } else {
      LOG(ERROR, "[EventPoll] unexpected event actived, fd: %d, event: %d", activeEvent_fd, activeEvent);
    }
  }
}

void EventPoll::eventDispatcher(std::shared_ptr<Event> event) {
  if(running) {
    switch(event->getType()) {
      case IO_EVENT:
        {
          IOEvent* activeIoEvent = event->getEventDetail().ioEvent;
          getEventQueue(activeIoEvent->getConnection())->addEvent(event);
        }
        break;

      case TIMEOUT_EVENT:
        broadcastEvent(event);
        break;

      case SUBCONNECTION_EVENT:
        {
          SubConnectionEvent* activeSubConnectionEvent = event->getEventDetail().subConnectionEvent;
          getEventQueue(activeSubConnectionEvent->getMasterConnection())->addEvent(event);
        }
        break;

      default:
        LOG(FATAL, "[EventPoll] handling unsupported event");
        ::exit(EXIT_FAILURE);
        break;
    }
  }
}

void EventPoll::shutdown() {
  running = false;
  for(auto eventLoop : eventLoopList) {
    eventLoop->shutdown();
  }
  eventLoopList.clear();
  eventQueueList.clear();
  for(auto &thread : eventLoopThreadList) {
    thread.join();
  }
  connectionSet.clear();
  thisPtr = nullptr;
  LOG(DEBUG, "[EventPoll] shut down");
}