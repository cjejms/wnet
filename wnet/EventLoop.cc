
#include "Connection.h"
#include "Event.h"
#include "EventLoop.h"
#include "EventQueue.h"
#include "TimeoutManager.h"

using namespace wnet;

void EventLoop::loop() {
  while(true) {
      // thread would be block when no events available to handle
    auto event = eventQueue->fetchEvent();

    switch(event->getType()) {
      case EventType::IO_EVENT:
        {
          // LOG(LogLevel::DEBUG, "[EventLoop][id %d] handling IO_EVENT", id);
          IOEvent* activeIoEvent = event->getEventDetail().ioEvent;
          auto connection = activeIoEvent->getConnection();
          LOG(LogLevel::DEBUG, "[EventLoop] loop id: %d, event_fd: %d, event: %d, start handling", id, connection->get_fd(), activeIoEvent->getEvent());
          connection->handleEvent(event);
          LOG(LogLevel::DEBUG, "[EventLoop] loop id: %d, event_fd: %d, event: %d, end handling", id, connection->get_fd(), activeIoEvent->getEvent());
        }
        break;

      case EventType::TIMEOUT_EVENT:
        {
          // LOG(LogLevel::DEBUG, "[EventLoop][id %d] handling TIMEOUT_EVENT", id);
          TimeoutEvent* activeTimeoutEvent = event->getEventDetail().timeoutEvent;
          timeoutManager->tickTock(activeTimeoutEvent->getExpireTimes());
        }
        break;

      case EventType::SUBCONNECTION_EVENT:
        {
          SubConnectionEvent* activeSubConnectionEvent = event->getEventDetail().subConnectionEvent;
          auto masterConnection = activeSubConnectionEvent->getMasterConnection();
          timeoutManager->clockIn(masterConnection->getTimeoutEntry());
          masterConnection->handleEvent(event);
        }
        break;
      
      case EventType::CONTROL_EVENT:
        {
          ControlEvent* activeControlEvent = event->getEventDetail().controlEvent;
          switch(activeControlEvent->getEvent()) {
            case ControlEventType::SHUT_DOWN:
              LOG(LogLevel::DEBUG, "[EventLoop][id %d] shutting down", id);
              return;
              break;

            default:
              LOG(LogLevel::FATAL, "[EventLoop][id %d] handling unsupported control event", id);
              ::exit(EXIT_FAILURE);
              break;
          }
        }
        break;

      default:
        LOG(LogLevel::FATAL, "[EventLoop][id %d] handling unsupported event", id);
        ::exit(EXIT_FAILURE);
        break;
    }
  }
}