#include "message/request.simpledata.pb.h"
#include "wnet.h"

using namespace wnet;

int main(int argc, char *argv[]) {

	SET_DAEMON_MODE();				
	// current directory will be changed to /
	
	// Log::setLogFile("{ PATH_TO_WNET }/wnet/log/simple_server_2");
	// log files will be put in {current directory}/log if not specified, 
	// directory will be created if not exist

	// Log::setLogLevel(INFO);
	// only log higher than or equal to level INFO will print

	auto server = std::make_shared<TCPServer>(10002);

	// singal handler for SIGINT (ctrl-c)
	Signal::setSignalHandler(SIGINT, [server]{
		server->shutdown();
	});

	// ignore SIGPIPE 
	Signal::setSignalHandler(SIGPIPE, []{});

	// this server do nothing
	server->setOnReceiveDataHandler(
		[](Connection* const connection) {
			connection->getInputBuffer()->clear();
		}
	);
	
	server->run();
	
	return 0;
}