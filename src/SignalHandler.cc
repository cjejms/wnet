#include "SignalHandler.h"

using namespace wnet;

std::map<int, std::function<void()>> Signal::SignalHandler;

void Signal::signal_handler_for_sys_call(int signalCode) {
  SignalHandler[signalCode]();
}

void Signal::setSignalHandler(int signalCode, const std::function<void()>& handler) {
  SignalHandler[signalCode] = handler;
  ::signal(signalCode, signal_handler_for_sys_call);
}