#include "wnet.h"

using namespace wnet;

int main(int argc, char *argv[]) {

	SET_DAEMON_MODE();				
	// current directory will be changed to /
	
	// Log::setLogFile("{ PATH_TO_WNET }/wnet/log/echo_server");
	// log files will be put in {current directory}/log if not specified, 
	// directory will be created if not exist

	// Log::setLogLevel(INFO);
	// only log higher than or equal to level INFO will print

	auto server = std::make_shared<TCPServer>(10007);

	// singal handler for SIGINT (ctrl-c)
	Signal::setSignalHandler(SIGINT, [server]{
		server->shutdown();
	});

	// ignore SIGPIPE 
	Signal::setSignalHandler(SIGPIPE, []{});

	server->setOnReceiveDataHandler(
		[](Connection* const connection) {
			connection->writeData(connection->getInputBuffer());
			connection->getInputBuffer()->clear();
		}
	);

	server->run();
	
	return 0;
}