
#include "Connection.h"
#include "Event.h"
#include "EventLoop.h"
#include "EventQueue.h"
#include "TimeoutManager.h"

using namespace wnet;

void EventLoop::loop() {
  while(running) {
    if(eventQueue->getQueueLen() > 0) {
      auto event = eventQueue->fetchEvent();

      switch(event->getType()) {
        case IO_EVENT:
          {
            // LOG(DEBUG, "[EventLoop][id %d] handling IO_EVENT", id);
            IOEvent* activeIoEvent = event->getEventDetail().ioEvent;
            auto connection = activeIoEvent->getConnection();
            LOG(DEBUG, "[EventLoop] loop id: %d, event_fd: %d, event: %d, start handling", id, connection->get_fd(), activeIoEvent->getEvent());
            connection->handleEvent(event);
            LOG(DEBUG, "[EventLoop] loop id: %d, event_fd: %d, event: %d, end handling", id, connection->get_fd(), activeIoEvent->getEvent());
          }
          break;

        case TIMEOUT_EVENT:
          {
            // LOG(DEBUG, "[EventLoop][id %d] handling TIMEOUT_EVENT", id);
            TimeoutEvent* activeTimeoutEvent = event->getEventDetail().timeoutEvent;
            timeoutManager->tickTock(activeTimeoutEvent->getExpireTimes());
          }
          break;

        case SUBCONNECTION_EVENT:
          {
            SubConnectionEvent* activeSubConnectionEvent = event->getEventDetail().subConnectionEvent;
            auto masterConnection = activeSubConnectionEvent->getMasterConnection();
            timeoutManager->clockIn(masterConnection->getTimeoutEntry());
            masterConnection->handleEvent(event);
          }
          break;
          
        default:
          LOG(FATAL, "[EventLoop][id %d] handling unsupported event", id);
          ::exit(EXIT_FAILURE);
          break;
      }
    } else {
      std::this_thread::sleep_for(std::chrono::milliseconds(EVENT_LOOP_SLEEP_MILLISECONDS));
    }
  }
  LOG(DEBUG, "[EventLoop][id %d] stop handling", id);
}