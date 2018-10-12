#pragma once

#include <functional>
#include <map>
#include <signal.h>

namespace wnet {

class Signal {
	public:
    static std::map<int, std::function<void()>> SignalHandler;

    // only for system call
    static void signal_handler_for_sys_call(int signalCode); 

    static void setSignalHandler(int signalCode, const std::function<void()>& handler);
  };

}