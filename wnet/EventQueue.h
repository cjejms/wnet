#pragma once

#include <condition_variable>
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
    std::condition_variable condVar;

  public:
    EventQueue(int _id):id(_id) {}

    ~EventQueue() {
  		LOG(LogLevel::DEBUG, "[EventQueue] destructing");
    }

    void addEvent(std::shared_ptr<Event> event) {
      std::lock_guard<std::mutex> lock(mutex);
      queue.push(event);
      // only one thread would try to fetch events from queue
      condVar.notify_one();
    }

    std::shared_ptr<Event> fetchEvent() {
      // block the thread when no events available to handle
      // would be woke up after addEvent() is called
      std::unique_lock<std::mutex> lock(mutex);
      condVar.wait(lock, [this]{
        return !queue.empty();
      });

      auto event = queue.front();
      queue.pop();
      return event;
    }

    size_t getQueueLen() {
      return queue.size();
    }

};

}