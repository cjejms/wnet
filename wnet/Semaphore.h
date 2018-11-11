#pragma once

#include <atomic>
#include <condition_variable>
#include <mutex>

namespace wnet {

class Semaphore {
  private:
  std::mutex mtx;
  std::condition_variable condVar;
  int count;
  
public:
  Semaphore(int _count = 0):count(_count) { }
 
  void signal() {
    std::unique_lock<std::mutex> lock(mtx);
    count++;
    condVar.notify_one();
  }
 
  void wait() {
    std::unique_lock<std::mutex> lock(mtx);
    condVar.wait(lock, [=] { return count > 0; });
    count--;
  }

};

}