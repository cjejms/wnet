#pragma once

#include <cstdio>
#include <cstring> 
#include <ctime>
#include <map>
#include <memory> 
#include <string> 
#include <dirent.h>
#include <sys/stat.h> 
#include <unistd.h>

#include "Config.h"

#define LOG(...) Log::addLog(__FILE__, __FUNCTION__, __LINE__, __VA_ARGS__)

namespace wnet{

enum LogLevel { DEBUG = 1, INFO, ERROR, FATAL };

class Log {
  private: 
    static LogLevel logLevel;
    static std::string logDirectory;
    static bool isAddLogToFile;
    static bool isWithLocation;

    static thread_local char contentBuf[LOG_BUF_SIZE];
    static thread_local char timeBuf[LOG_TIME_BUF_SIZE];
    static thread_local char fileNameBuf[LOG_FILE_NAME_BUF_SIZE];
    static thread_local char locationBuf[LOG_FILE_LOCATION_BUF_SIZE];
    
    static thread_local FILE* logFile;

    static const char* getLevelStr(LogLevel level) {
      switch(level) {
        case DEBUG:
          return "[DEBUG]";
        case INFO:
          return "[INFO ]";
        case ERROR:
          return "[ERROR]";
        case FATAL:
          return "[FATAL]";
      }
      return nullptr;
    }
    
    static void getTime(const char* timeFormat) {
      time_t now = ::time(NULL);
      struct tm tm;
      ::localtime_r(&now, &tm);
      ::strftime(timeBuf, LOG_TIME_BUF_SIZE, timeFormat, &tm);
    }

    static void getLocation(const char* fileName, const char* funcName, int lineNo) {
      ::snprintf(locationBuf, LOG_FILE_LOCATION_BUF_SIZE, "[file:%s,func:%s,line:%d]", fileName, funcName, lineNo);
    }

    static void openLogFile() {
      getTime("%m_%d_%H");
      ::snprintf(fileNameBuf, LOG_FILE_NAME_BUF_SIZE, "%s%s%s%s", logDirectory.c_str(), "/", timeBuf, ".log");
      logFile = ::fopen(fileNameBuf, "a");
      if(!logFile) {
        isAddLogToFile = false;
        LOG(FATAL, "[Log][logFile %s] open log file failed", fileNameBuf);
        ::exit(EXIT_FAILURE);
      }
    }

  public:
    static void setLogLevel(LogLevel level) {
      logLevel = level;
    }

    static void setLogWithLocation() {
      isWithLocation = true;
    }

    static void setLogFile(std::string _logDirectory = "") {
      if(_logDirectory.empty()) {
        ::strcat(::getcwd(fileNameBuf, LOG_FILE_NAME_BUF_SIZE), "/log");
        logDirectory = fileNameBuf;
      } else {
        logDirectory = _logDirectory;
      }
      const char* directory = logDirectory.c_str();
      // check if log directory exist, create it if not 
      DIR *dir = ::opendir(directory);
      if(!dir) {
        // log directory not exist
        LOG(INFO, "[Log][logDirectory %s] logDirectory not exist, creating...", directory);
        if(::mkdir(directory, S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) == -1) {
          LOG(FATAL, "[Log][logDirectory %s] unable to create logDirectory", directory);
          ::exit(EXIT_FAILURE);
        } else {
          LOG(INFO, "[Log][logDirectory %s] logDirectory created", directory);
        }
      } else {
        ::closedir(dir);
      }
      isAddLogToFile = true;
    }

    template <typename... Args>
    static void addLog(const char* fileName, const char* funcName, int lineNo, LogLevel level, const char* context, Args... args) {
      if(isAddLogToFile) {
        openLogFile();
      }
      if(level >= logLevel) {
        getTime("[%m-%d %T]");
        getLocation(fileName, funcName, lineNo);
        const char* levelStr = getLevelStr(level);

        if(::strlen(levelStr) + ::strlen(timeBuf) + ::strlen(context) + (isWithLocation ? ::strlen(locationBuf) : 0) + 3 > LOG_BUF_SIZE) {
          ::printf("[log error] log context too long @ %s\n", locationBuf);
          return;
        } else {
          isWithLocation ?  ::snprintf(contentBuf, LOG_BUF_SIZE, "%s%s%s %s%s", levelStr, timeBuf, locationBuf, context, "\n")
                          : ::snprintf(contentBuf, LOG_BUF_SIZE, "%s%s %s%s", levelStr, timeBuf, context, "\n");
          if(sizeof...(args) == 0) {
            isAddLogToFile ? ::fprintf(logFile, contentBuf) : ::printf(contentBuf);
          } else {
            isAddLogToFile ? ::fprintf(logFile, contentBuf, args...) : ::printf(contentBuf, args...);
          }
        }
      }
      if(isAddLogToFile && logFile) {
        ::fclose(logFile);
      }
    }

};

}