# wnet

wnet is an event-driven multithreading TCP server written in C++11

* work on Linux only, rely on epoll and timerfd for core
* write in C++11, manage resources and objects with smart pointer
* master thread in charge of controlling the server, handling new connections and dispatching events to worker threads
* worker threads keep on fetching queued events and handle them
* all connection, passive or active, are constructed asynchronously
* use google protobuf::Message as request/response data structure
* worker threads amount configurable to make full use of multi-core CPU


## example

### an echo server

```cpp
// example/echo_server.cc
// ......
auto server = std::make_shared<TCPServer>(10007);
// ......
server->setOnReceiveDataHandler(
  [](Connection* const connection) {
    connection->writeData(connection->getInputBuffer());
    connection->getInputBuffer()->clear();
  }
);

server->run();
```

### a server with subrequests

```cpp
// example/complex_server.cc
// ......
auto server = std::make_shared<TCPServer>(10004);

server->setOnReceiveDataHandler(
  [](Connection* const masterConnection) {
    // ......
    // server code in example/simple_server_1.cc
    // connect to 127.0.0.1:10001
    auto requestResult1 = Connector::initSubRequest("127.0.0.1", 
                                                    10001, 
                                                    requestData, 
                                                    masterConnectionPtr);
    
    // server code in example/simple_server_2.cc
    // connect to 127.0.0.1:10002
    // server 127.0.0.1:10002 response nothing and will reject on timeout (3 s, 
    // SUB_REQUEST_TIMEOUT_SECONDS by default, configurable in wnet/Config.h)
    auto requestResult2 = Connector::initSubRequest("127.0.0.1", 
                                                    10002, 
                                                    requestData, 
                                                    masterConnectionPtr,
                                                    3);
    
    // server code in example/simple_server_3.cc
    // connect to 127.0.0.1:10003
    auto requestResult3 = Connector::initSubRequest("127.0.0.1", 
                                                    10003, 
                                                    requestData, 
                                                    masterConnectionPtr);

    // await subrequest and call back
    masterConnection->await(
      {requestResult1, requestResult2}, 		
      // this handler will be called after all the subrequests below resolved or rejected
      [=](Connection* const masterConnection) {
        // handle response data from requestResult1
        if(requestResult1->resolved() || requestResult1->getData()) {
          // ......
        } else {
          // ......
        }
        // handle response data from requestResult2
        // ......
        
        masterConnection->await(
          {requestResult3}, 
          [=](Connection* const masterConnection) {
            // handle response data from requestResult3
            // ......

            // connect to 127.0.0.1:10003 again
            auto requestResult4 = Connector::initSubRequest("127.0.0.1", 
                                                            10003, 
                                                            requestData, 
                                                            masterConnectionPtr);
            masterConnection->await(
              {requestResult4}, 
              [=](Connection* const masterConnection) {
                // handle response data from requestResult4
                // ......
                
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
```
