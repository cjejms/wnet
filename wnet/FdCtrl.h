#pragma once

#include <cerrno>
#include <cstdlib>
#include <fcntl.h>
#include <sys/socket.h>

#include "Log.h"

namespace wnet {

class FdCtrl {
  public:
    static void setNoneBlock(int fd) {
      int opts = ::fcntl(fd, F_GETFL);    
      if(::fcntl(fd, F_SETFL, opts | O_NONBLOCK) == -1) {
        LOG(LogLevel::FATAL, "[Util][fd %d][fcntl()] set fd nonblocking failed, error: [%d]%s", fd, errno, ::strerror(errno));
        ::exit(EXIT_FAILURE);
      } 
    }

    static void setReuseAddr(int fd) {
      int flag = true;
      if(::setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &flag, sizeof flag) == -1) {
        LOG(LogLevel::FATAL, "[Util][fd %d][setsockopt()] set socket reuse addr failed, error: [%d]%s", fd, errno, ::strerror(errno));
        ::exit(EXIT_FAILURE);
      } 
    }

    static void setReusePort(int fd) {
      int flag = true;
      if(::setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &flag, sizeof flag) == -1) {
        LOG(LogLevel::FATAL, "[Util][fd %d][setsockopt()] set socket reuse port failed, error: [%d]%s", fd, errno, ::strerror(errno));
        ::exit(EXIT_FAILURE);
      } 
    }

    static void addFdFlag(int fd, int flag) {
      int ret = ::fcntl(fd, F_GETFD);
      if(::fcntl(fd, F_SETFD, ret | flag) == -1) {
        LOG(LogLevel::FATAL, "[Util][fd %d][fcntl()] add fd flag failed, flag %d, error: [%d]%s", fd, flag, errno, ::strerror(errno));
        ::exit(EXIT_FAILURE);
      } 
    }

};

}