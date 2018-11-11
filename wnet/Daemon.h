#pragma once

#include <cstdlib>
#include <signal.h>
#include <sys/stat.h>
#include <unistd.h>

#include "Config.h"
#include "Log.h"

#define SET_DAEMON_MODE() Daemon::setDaemonMode(__FILE__)

namespace wnet {

class Daemon {
  public:
  // code from https://stackoverflow.com/questions/17954432/creating-a-daemon-in-linux
    static void setDaemonMode(const char* serverName) {
      /* Fork off the parent process */
      pid_t pid = ::fork();
      
      /* An error occurred */
      if (pid < 0)
        ::exit(EXIT_FAILURE);

      /* Success: Let the parent terminate */
      if (pid > 0)
        ::exit(EXIT_SUCCESS);

      /* On success: The child process becomes session leader */
      if (::setsid() < 0)
        ::exit(EXIT_FAILURE);

      /* Catch, ignore and handle signals */
      //TODO: Implement a working signal handler */
      ::signal(SIGCHLD, SIG_IGN);
      ::signal(SIGHUP, SIG_IGN);

      /* Fork off for the second time*/
      pid = ::fork();

      /* An error occurred */
      if (pid < 0)
        ::exit(EXIT_FAILURE);

      /* Success: Let the parent terminate */
      if (pid > 0)
        ::exit(EXIT_SUCCESS);

      /* Set new file permissions */
      ::umask(0);

      /* Change the working directory to the root directory */
      /* or another appropriated directory */
      ::chdir("/");

      /* Close all open file descriptors */
      for (int x = static_cast<int>(::sysconf(_SC_OPEN_MAX)); x>=0; x--) {
        ::close(x);
      }

      // write process ID to file
      char fileNameBuf[LOG_FILE_NAME_BUF_SIZE];
      ::getcwd(fileNameBuf, LOG_FILE_NAME_BUF_SIZE);
      ::strcat(fileNameBuf, "/");
      ::strcat(fileNameBuf, serverName);
      ::strcat(fileNameBuf, ".pid");
      FILE* pidFile = ::fopen(fileNameBuf,"w+");
      if(!pidFile) {
        LOG(LogLevel::FATAL, "[Daemon] open pid file failed, pid file path: %s", fileNameBuf);
        ::exit(EXIT_FAILURE);
      }
      ::fprintf(pidFile, "%d", static_cast<int>(::getpid()));
      ::fclose(pidFile);
      LOG(LogLevel::INFO, "[Daemon] running in Daemon mode");
    }
};

}