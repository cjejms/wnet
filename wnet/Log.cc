#include "Log.h"

using namespace wnet;

LogLevel Log::logLevel = DEBUG;
std::string Log::logDirectory = "";
bool Log::isAddLogToFile = false;
bool Log::isWithLocation = false;

thread_local char Log::contentBuf[LOG_BUF_SIZE];
thread_local char Log::timeBuf[LOG_TIME_BUF_SIZE];
thread_local char Log::fileNameBuf[LOG_FILE_NAME_BUF_SIZE];
thread_local char Log::locationBuf[LOG_FILE_LOCATION_BUF_SIZE];

thread_local FILE* Log::logFile = nullptr;