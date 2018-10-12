#pragma once

#include <memory>

#include "Log.h"

namespace wnet {

class Connection;

enum IOEventType { READ_EVENT = 1, WRITE_EVENT, READ_AND_WRITE_EVENT };
class IOEvent {
  private:
    const int event_fd = -1;
    IOEventType ioType;
    std::shared_ptr<Connection> connection;

  public:
    IOEvent(int _event_fd, 
            IOEventType _ioType, 
            std::shared_ptr<Connection> _connection = nullptr): event_fd(_event_fd), 
                                                                ioType(_ioType),
                                                                connection(_connection) {}

    ~IOEvent() {}

    int get_fd() {
      return event_fd;
    }
    
    IOEventType getEvent() {
      return ioType;
    }

    std::shared_ptr<Connection> getConnection() {
      return connection;
    }
};

class TimeoutEvent {
  private:
    const uint64_t expireTimes = 0;

  public:
    TimeoutEvent(uint64_t _expireTimes):expireTimes(_expireTimes) {}

    ~TimeoutEvent() {}

    uint64_t getExpireTimes() {
      return expireTimes;
    }
};

enum SubConnectionEventType { PENDING = 1, RESOLVED, REJECTED };
class SubConnectionEvent {
  private:
    std::shared_ptr<Connection> masterConnection;
    std::shared_ptr<Connection> subConnection;
    SubConnectionEventType eventType;

  public:
    SubConnectionEvent( std::shared_ptr<Connection> _masterConnection,
                        std::shared_ptr<Connection> _subConnection,
                        SubConnectionEventType _eventType): masterConnection(_masterConnection),
                                                            subConnection(_subConnection),
                                                            eventType(_eventType) {}

    ~SubConnectionEvent() {}

    std::shared_ptr<Connection> getMasterConnection() {
      return masterConnection;
    }

    std::shared_ptr<Connection> getSubConnection() {
      return subConnection;
    }

    SubConnectionEventType getEvent() {
      return eventType;
    }
};

union EventDetail {
  IOEvent*            ioEvent;
  TimeoutEvent*       timeoutEvent;
  SubConnectionEvent* subConnectionEvent;
};

enum EventType { IO_EVENT = 1, TIMEOUT_EVENT, SUBCONNECTION_EVENT };
class Event {
  private:
    const EventType type;
    EventDetail eventDetail;

  public:
    Event(EventType _type, EventDetail _eventDetail): type(_type), 
                                                      eventDetail(_eventDetail) {} 

    ~Event() {
      switch(type) {
        case IO_EVENT:
          delete eventDetail.ioEvent;
          break;
          
        case TIMEOUT_EVENT:
          delete eventDetail.timeoutEvent;
          break;

        case SUBCONNECTION_EVENT:
          delete eventDetail.subConnectionEvent;
          break;
      }
       
    }  

    EventType getType() {
      return type;
    }

    EventDetail getEventDetail() {
      return eventDetail;
    }
};

}