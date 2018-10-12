#pragma once

#include <algorithm>
#include <array>
#include <memory>
#include <mutex>
#include <set>

#include "Config.h"
#include "Log.h"
#include "Timer.h"

namespace wnet {

class Connection;

class TimeoutEntry {
  private: 
    std::weak_ptr<Connection> connection;
    std::function<void()> timeoutHandler;

  public:
    TimeoutEntry( std::shared_ptr<Connection> _connection,
                  std::function<void()> _timeoutHandler = nullptr): connection(_connection), 
                                                                    timeoutHandler(_timeoutHandler) {}

    ~TimeoutEntry();

    std::shared_ptr<Connection> getConnection();
    
};

class TimeoutManager {
  private:
    std::array<std::set<std::shared_ptr<TimeoutEntry>>, CONNECTION_MAX_SURVIVE_TICK_TOCK> TimeoutCircle;
    int index = 0;
    std::mutex mutx;

  public:
    TimeoutManager() {}

    ~TimeoutManager() {}

    void setTimeout(int seconds, 
                    std::shared_ptr<Connection> connection, 
                    std::function<void()> timeoutHandler) {
      TimeoutCircle[
        (std::min({ seconds / TIME_ACCURACY, CONNECTION_MAX_SURVIVE_TICK_TOCK }) + index) % CONNECTION_MAX_SURVIVE_TICK_TOCK
        ].insert(
        std::make_shared<TimeoutEntry>(connection, timeoutHandler)
      );
    }

    void clockIn(std::shared_ptr<TimeoutEntry> entry) {
      if(entry) { // not nullptr
        std::lock_guard<std::mutex> guard(mutx);
        if(index == 0) {
          TimeoutCircle[CONNECTION_MAX_SURVIVE_TICK_TOCK - 1].insert(entry);
        } else {
          TimeoutCircle[index - 1].insert(entry);
        }
      }
    }

    void tickTock(uint64_t expireTimes) {
      for(uint64_t i = 0; i < expireTimes; i++) {
        TimeoutCircle[index].clear();
        index += 1;
        if(index == CONNECTION_MAX_SURVIVE_TICK_TOCK) {
          index = 0;   // make it circle
        }
      }
    }
};

}
