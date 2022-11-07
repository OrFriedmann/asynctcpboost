
# TCP ASYNC SERVER

This Project contains 2 main components
Authenticatation server to authenticate requests.
Proxy server to get authentication requests and forward them to the Authentication server.
The proxy will cache the request for 60 seconds before give up.
Each request should end with ';' and should contains \<username>:\<password>
request should be \<username>:\<password>;
To **compile** this project you should install first libboost on your pc.
compile and create binary for the proxy and the authentication server.

    g++ -c LRU.cpp Authenticator.cpp Proxy.cpp Client.cpp -lboost_system -lboost_date_time -lboost_thread

g++ Proxy.o  -o proxy;
g++ Authenticator.o -o auth_server;

# TODO
Change the Async mechanism from boost::asio to epoll()
Improve Error handling
