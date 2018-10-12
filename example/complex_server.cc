#include "message/request.simpledata.pb.h"
#include "wnet.h"

using namespace wnet;

int main(int argc, char *argv[]) {

	// require simple_server_1, simple_server_2, simple_server_3 to run

	// SET_DAEMON_MODE();				
	// current directory will be changed to /
	
	// Log::setLogFile();		
	// log files will be put in {current directory}/log if not specified, 
	// directory will be created if not exist

	// Log::setLogLevel(INFO);
	// only log higher than or equal to level INFO will print
	// Log::setLogWithLocation();

	auto server = std::make_shared<TCPServer>(10004);

	// singal handler for SIGINT (ctrl-c)
	Signal::setSignalHandler(SIGINT, [server]{
		server->shutdown();
	});

	// ignore SIGPIPE 
	Signal::setSignalHandler(SIGPIPE, []{});
	
	server->setOnReceiveDataHandler(
		[](Connection* const masterConnection) {
			masterConnection->getInputBuffer()->clear();

			auto masterConnectionPtr = masterConnection->thisConnection();

			auto requestData = std::make_shared<request::simpledata>();
			requestData->set_id(42);
			requestData->set_msg("origin msg");
			
			// connect to 127.0.0.1:10001
			auto requestResult1 = Connector::initSubRequest("127.0.0.1", 
																											10001, 
																											requestData, 
																											masterConnectionPtr);
			// connect to 127.0.0.1:10002
			// server 127.0.0.1:10002 response nothing and will reject on timeout (3 s)
			auto requestResult2 = Connector::initSubRequest("127.0.0.1", 
																											10002, 
																											requestData, 
																											masterConnectionPtr,
																											3);
			// connect to 127.0.0.1:10003
			auto requestResult3 = Connector::initSubRequest("127.0.0.1", 
																											10003, 
																											requestData, 
																											masterConnectionPtr);

			// await subrequest and call back
			masterConnection->await(
				{requestResult1, requestResult2}, 		
				// after all the subrequests below are resolved or rejected, the follwing handler will be called
				[=](Connection* const masterConnection) {
					if(requestResult1->resolved() || requestResult1->getData()) {
						auto message1 = std::static_pointer_cast<request::simpledata>(requestResult1->getData());
						masterConnection->writeData(std::to_string(message1->id()));
						masterConnection->writeData(": ");
						masterConnection->writeData(message1->msg());
						masterConnection->writeData("\n");
					} else {
						masterConnection->writeData("request to 127.0.0.1:10001 rejected\n");
					}
					
					if(requestResult2->resolved() || requestResult2->getData()) {
						auto message2 = std::static_pointer_cast<request::simpledata>(requestResult2->getData());
						masterConnection->writeData(std::to_string(message2->id()));
						masterConnection->writeData(": ");
						masterConnection->writeData(message2->msg());
						masterConnection->writeData("\n");
					} else {
						masterConnection->writeData("request to 127.0.0.1:10002 rejected\n");
					}
					
					masterConnection->await(
						{requestResult3}, 
						[=](Connection* const masterConnection) {
							if(requestResult3->resolved() || requestResult3->getData()) {
								auto message3 = std::static_pointer_cast<request::simpledata>(requestResult3->getData());
								masterConnection->writeData(std::to_string(message3->id()));
								masterConnection->writeData(": ");
								masterConnection->writeData(message3->msg());
								masterConnection->writeData("\n");
							} else {
								masterConnection->writeData("request to 127.0.0.1:10003 rejected\n");
							}

							// connect to 127.0.0.1:10003 again
							requestData->set_id(666);
							auto requestResult4 = Connector::initSubRequest("127.0.0.1", 
																															10003, 
																															requestData, 
																															masterConnectionPtr);
							masterConnection->await(
								{requestResult4}, 
								[=](Connection* const masterConnection) {
									if(requestResult4->resolved() || requestResult4->getData()) {
										auto message3 = std::static_pointer_cast<request::simpledata>(requestResult4->getData());
										masterConnection->writeData(std::to_string(message3->id()));
										masterConnection->writeData(": ");
										masterConnection->writeData(message3->msg());
										masterConnection->writeData("\n");
									} else {
										masterConnection->writeData("request to 127.0.0.1:10003 again rejected\n");
									}
									
									// since requestResult3 has resolved earlier
									// requestResult4 should have resued the connection
									if(requestResult3->getSubConnection() == requestResult4->getSubConnection()) {
										masterConnection->writeData("subconnection reused\n");
									} else {
										masterConnection->writeData("subconnection not reused\n");
									}

									// Connection::requestResolved() must be called to get connection ready to handle new request
									// instead of keep waiting for sub connection to resolve
									masterConnection->requestResolved();
								}
							);
							
						}
					);

				}
			);

		}
	);

	server->run();
		
  return 0;
}
