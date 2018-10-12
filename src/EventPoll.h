#pragma once

#include <cerrno>
#include <cstdlib>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <vector>
#include <sys/epoll.h>
#include <unistd.h>

#include "Config.h"
#include "Log.h"
#include "Noncopyable.h"

namespace wnet {

class Connection;
class Event;
class EventLoop;
class EventQueue;
class TCPServer;
class Timer;

class EventPoll : public noncopyable, public std::enable_shared_from_this<EventPoll> {
	private:
		int epoll_fd;
		struct epoll_event readyEvents[MAX_READY_EVENT_PER_POLL];
		bool running = false;

		// different threads share one file descriptor set
		// so only one EvevtPoll instance every process
		static std::shared_ptr<EventPoll> thisPtr;
		static std::mutex mutx;

		int server_fd = -1;
		std::weak_ptr<TCPServer> server;

		int timer_fd = -1;
		std::shared_ptr<Timer> timer;

		std::map<int, std::shared_ptr<Connection>> connectionSet;   // connection fd => Connection ptr
		std::vector<std::shared_ptr<EventQueue>> eventQueueList; 
		std::vector<std::shared_ptr<EventLoop>> eventLoopList; 
		std::vector<std::thread> eventLoopThreadList;   

		void epollCtrl(int operation, int event_fd, int event) {
			struct epoll_event epollEvent;
			epollEvent.data.fd = event_fd;
			epollEvent.events = event;
			if(::epoll_ctl(epoll_fd, operation, event_fd, &epollEvent) == -1) {
				LOG(FATAL, "[EventPoll][fd %d][epoll_ctl()] edit fd event register failed, event: %d, error: [%d]%s", event_fd, event, errno, ::strerror(errno));
				::exit(EXIT_FAILURE);
			}
		}

		EventPoll();
		
		std::shared_ptr<EventLoop> getEventLoop(std::shared_ptr<Connection> connection);

		std::shared_ptr<EventQueue> getEventQueue(std::shared_ptr<Connection> connection);

		void broadcastEvent(std::shared_ptr<Event> event);

	public: 
		static const int READ_EVENT    = EPOLLIN ;
    static const int WRITE_EVENT   = EPOLLOUT;
    static const int EDGE_TRIGGER  = EPOLLET ;
    static const int LEVEL_TRIGGER = 0			 ;

		// different threads share one file descriptor set
		// so only one EvevtPoll instance every process
		static std::shared_ptr<EventPoll> getInstance();

		~EventPoll() {
  		LOG(DEBUG, "[EventPoll] destructing");
			::close(epoll_fd);
		}

		void setServer(int _server_fd, std::shared_ptr<TCPServer> _server);

    void addEventListener(int event_fd, int event, int triggerMode = EDGE_TRIGGER) {
      epollCtrl(EPOLL_CTL_ADD, event_fd, event | triggerMode);
    };
 
    void removeEventListener(int event_fd) {
      epollCtrl(EPOLL_CTL_DEL, event_fd, 0);	 // third param is not so necessary, just for compatibility
    };

    void updateEventListener(int event_fd, int event, int triggerMode = EDGE_TRIGGER) {
      epollCtrl(EPOLL_CTL_MOD, event_fd, event | triggerMode);
    };

		void addConnection(std::shared_ptr<Connection> connection);	

		std::shared_ptr<Connection> getConnection(int connection_fd);

		void removeConnection(int connection_fd);

		int getConnectionCount() {
			return static_cast<int>(connectionSet.size());
		}

		void connectionIniIdleTimeout(std::shared_ptr<Connection> connection);

		void connectionClockIn(std::shared_ptr<Connection> connection);

		void setTimeout( int seconds, std::shared_ptr<Connection> connection, std::function<void()> timeoutHandler);

		void poll();

		void eventDispatcher(std::shared_ptr<Event> event);

		void shutdown();

		

		

};

}