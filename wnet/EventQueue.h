#pragma once

#include <memory>
#include <mutex>
#include <queue>

#include "Log.h"
#include "Noncopyable.h"

namespace wnet {

class Event;

class EventQueue : public noncopyable {
  private: 
    int id; // for debug
    std::queue<std::shared_ptr<Event>> queue;   
    std::mutex mutex;

  public:
    EventQueue(int _id):id(_id) {}

    ~EventQueue() {
  		LOG(DEBUG, "[EventQueue] destructing");
    }

    void addEvent(std::shared_ptr<Event> event) {
      std::lock_guard<std::mutex> lock(mutex);
      queue.push(event);
    }

    std::shared_ptr<Event> fetchEvent() {
      auto event = queue.front();
      queue.pop();
      return event;
    }

    size_t getQueueLen() {
      return queue.size();
    }

};

}