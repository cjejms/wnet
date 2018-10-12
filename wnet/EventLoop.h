#pragma once

#include <chrono>
#include <memory>
#include <thread>

#include "Config.h"
#include "Log.h"
#include "Noncopyable.h"

namespace wnet {

class EventQueue;
class TimeoutManager;

class EventLoop : public noncopyable {
  private: 
    int id;  
    bool running = false;

    std::shared_ptr<EventQueue> eventQueue;
    std::shared_ptr<TimeoutManager> timeoutManager;

  public:
    EventLoop(int _id, std::shared_ptr<EventQueue> _queue): id(_id), 
                                                            running(true),
                                                            eventQueue(_queue) {
      timeoutManager = std::make_shared<TimeoutManager>();
    }

    ~EventLoop() {
      LOG(DEBUG, "[EventLoop] destructing");
    }

    int getID() {
      return id;
    }

    void shutdown() {
      running = false;
    }
    
    std::shared_ptr<TimeoutManager> getTimeoutManager() {
      return timeoutManager;
    }
    
    // work in separated thread, loop untill shut down
    void loop();
    
};

}