#include "message/request.simpledata.pb.h"
#include "wnet.h"

using namespace wnet;

int main(int argc, char *argv[]) {

	SET_DAEMON_MODE();				
	// current directory will be changed to /
	
	// Log::setLogFile("{ PATH_TO_WNET }/wnet/log/simple_server_3");
	// log files will be put in {current directory}/log if not specified, 
	// directory will be created if not exist

	// Log::setLogLevel(INFO);
	// only log higher than or equal to level INFO will print

	auto server = std::make_shared<TCPServer>(10003);

	// singal handler for SIGINT (ctrl-c)
	Signal::setSignalHandler(SIGINT, [server]{
		server->shutdown();
	});

	// ignore SIGPIPE 
	Signal::setSignalHandler(SIGPIPE, []{});

	server->setOnReceiveDataHandler(
		[](Connection* const connection) {
			ParseResult parseResult;
			auto rawMessage = connection->decodeMessage(parseResult);

			if(parseResult == PARSE_SUCCESS) {
				auto message = std::static_pointer_cast<request::simpledata>(rawMessage);
				message->set_id(message->id() + 3);
				message->set_msg("data processed by simple server 3");
				connection->writeData(message);
			} else {
				if(parseResult == UNKNOWN_MESSAGE_TYPE || parseResult == PARSE_ERROR) {
					auto message = std::make_shared<request::simpledata>();
					message->set_id(0);
					message->set_msg("bad data for simple server 3");
					connection->writeData(message);
				}
			}
		}
	);
	
	server->run();
	
	return 0;
}