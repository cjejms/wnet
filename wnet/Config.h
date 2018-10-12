#pragma once

namespace wnet {

// used in Buffer
constexpr int DEFAULT_BUFFER_SIZE = 1024;

// used in Connector
constexpr int SUB_REQUEST_TIMEOUT_SECONDS = 6;  

// used in EventLoop
constexpr int EVENT_LOOP_SLEEP_MILLISECONDS = 400;   // milliseconds

// used in EventPoll
constexpr int EVENT_LOOP_COUNT = 2;  
constexpr int MAX_READY_EVENT_PER_POLL = 20;
constexpr int EPOLL_WAIT_TIMEOUT = 500;   // milliseconds

// used in Log
constexpr int LOG_BUF_SIZE = 512;
constexpr int LOG_TIME_BUF_SIZE = 64;
constexpr int LOG_FILE_NAME_BUF_SIZE = 256;
constexpr int LOG_FILE_LOCATION_BUF_SIZE = 256;

// used in TCPServer
constexpr int SERVER_LISTEN_QUEUE_LENGTH = 20;

// used in Timer
constexpr int TIME_ACCURACY = 3;    // seconds

// used in TimeoutManager
constexpr int CONNECTION_MAX_SURVIVE_TICK_TOCK = 10;   
//  max surviving time length for an idle connection = CONNECTION_MAX_SURVIVE_TICK_TOCK X TIME_ACCURACY

}