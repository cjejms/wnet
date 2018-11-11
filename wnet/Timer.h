#pragma once

#include <cerrno>
#include <sys/timerfd.h>
#include <unistd.h>

#include "Config.h"
#include "FdCtrl.h"
#include "Log.h"
#include "Noncopyable.h"

namespace wnet {

class Timer : public noncopyable {  
  private:
    uint64_t expireTimes;           // store times of expiration, restore to 0 everytime before handle
    uint64_t expireTimesSinceInit;   
    int timer_fd;
    struct itimerspec timerDetail;

    void setTimerDetail(int interval = TIME_ACCURACY) {
      timerDetail.it_interval.tv_sec = interval;
      timerDetail.it_interval.tv_nsec = 0;
      timerDetail.it_value.tv_sec = interval;
      timerDetail.it_value.tv_nsec = 0;
      // tv_sec tv_nsec
      // Setting either field of timerDetail.it_value to a nonzero value arms the timer.
      // Setting both fields of timerDetail.it_value to zero disarms the timer.
      if(::timerfd_settime(timer_fd, 0, &timerDetail, NULL) == -1) {
        LOG(LogLevel::FATAL, "[Timer][timerfd_settime()] specify timer failed, error: [%d]%s", errno, ::strerror(errno));
        ::exit(EXIT_FAILURE);
      }
    }

  public:
    Timer():expireTimes(0), expireTimesSinceInit(0) {
      timer_fd = ::timerfd_create(CLOCK_MONOTONIC, 0);   // a nonsettable clock that is not affected by discontinuous changes in the system clock
      if(timer_fd == -1) {
        LOG(LogLevel::FATAL, "[Timer][timerfd_create()] timer_fd initialize failed, error: [%d]%s", errno, ::strerror(errno));
        ::exit(EXIT_FAILURE);
      }
    }

    ~Timer() {
      ::close(timer_fd);
    }

    uint64_t handle() {
      expireTimes = 0;
      size_t readResult = ::read(timer_fd, &expireTimes, sizeof expireTimes);
      if(readResult <= 0) {
        LOG(LogLevel::FATAL, "[Timer][read()] something's wrong, error: [%d]%s", errno, ::strerror(errno));
      }
      expireTimesSinceInit += expireTimes;
      return expireTimes;
    }

    void startTickTock() {
      FdCtrl::setNoneBlock(timer_fd);
      setTimerDetail();
    }
    
    int get_fd() {
      return timer_fd;
    }
};

}