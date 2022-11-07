#include "MemAlloc.cpp"
#include "LRU.cpp"
#include <boost/asio.hpp>
#include <optional>
#include <iostream>
#include <vector>
#include <atomic>
class AuthenticatorServer {

};

class session : public std::enable_shared_from_this<session>
{
    public:
        session(boost::asio::ip::tcp::socket&& socket, boost::asio::ip::tcp::socket&& socket_auth, LRU* _lru)
        : socket(std::move(socket)), socket_auth(std::move(socket_auth)),streambuf(512), lru(_lru)
        {}

        void read_req()
        {   
            streambuf.consume(512);
            boost::asio::async_read_until(socket, streambuf, ';', [self = shared_from_this()] (boost::system::error_code error, std::size_t bytes_transferred)
            {
                std::string buf = {buffers_begin(self->streambuf.data()), buffers_end(self->streambuf.data())};
                bool in_cache = self->lru->get(std::string(buf));
                if(!error && !in_cache)
                    self->check_req(const_cast<char *>(boost::asio::buffer_cast<const char*>(self->streambuf.data())), bytes_transferred);
                if(in_cache)
                {
                    std::string creds = {buffers_begin(self->streambuf.data()), buffers_end(self->streambuf.data())};
                    self->streambuf.consume(512);
                    std::ostream resp(&self->streambuf);
                    resp << "ok";
                    self->write_back(2, creds, true);
                }
            });
        }

        void write_back(std::size_t bytes, const std::string& creds, bool in_cache = false)
        {
            std::string resp = {buffers_begin(streambuf.data()), buffers_end(streambuf.data())};
            if(resp == "ok" && !in_cache)
            {
                lru->put(creds);
            }
            boost::asio::async_write(socket, boost::asio::buffer(boost::asio::buffer_cast<const char*>(streambuf.data()),bytes), [self = shared_from_this()] (boost::system::error_code error, std::size_t bytes_transferred)
            {
                if(error)
                    std::cerr << error.message() << std::endl;
                self->read_req();
            });

        }

        void check_req(char* buffer, size_t bytes) {
            boost::asio::async_write(socket_auth, boost::asio::buffer(buffer, bytes), [self = shared_from_this()] (boost::system::error_code error, std::size_t bytes_transfered) 
            {
                if(!error)
                {
                    self->read_auth_resp();
                }
            });
        }

        void read_auth_resp(bool in_cache = false) {
            std::string creds = {buffers_begin(streambuf.data()), buffers_end(streambuf.data())};
            streambuf.consume(512);
            if(!in_cache)
                boost::asio::async_read(socket_auth, streambuf, boost::asio::transfer_at_least(2),[self = shared_from_this(), creds=creds] (boost::system::error_code error, std::size_t bytes_transfered) 
                {
                    if(!error)
                    {
                        self->write_back(bytes_transfered, creds);
                    }
                });
            else {
                std::ostream resp(&streambuf);
                resp << "ok";
                write_back(2, creds, true);
            }
        }


    private:
        boost::asio::ip::tcp::socket socket;
        boost::asio::ip::tcp::socket socket_auth;
        boost::asio::streambuf streambuf;
        std::vector<std::string> allowed_users;
        LRU* lru;
};

class Server
{
    public:
    
        Server(boost::asio::io_context& io_context, std::uint16_t port, LRU* _lru)
        : io_context(io_context)
        , acceptor  (io_context, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port)), lru(_lru)
        {

        }
    
        void async_accept()
        {
            socket.emplace(io_context);
            socket_auth.emplace(io_context);
            boost::asio::ip::address address = boost::asio::ip::make_address("127.0.0.1");
            boost::asio::ip::tcp::endpoint endpoint(address, 15001);
            boost::system::error_code error;
            socket_auth->connect(endpoint, error);
            acceptor.async_accept(socket.value(), [&] (boost::system::error_code error)
            {
                std::make_shared<session>(std::move(*socket), std::move(*socket_auth), lru)->read_req();
                async_accept();
            });
        }
    
    private:
    
        boost::asio::io_context& io_context;
        boost::asio::ip::tcp::acceptor acceptor;
        std::optional<boost::asio::ip::tcp::socket> socket;
        std::optional<boost::asio::ip::tcp::socket> socket_auth;
        LRU* lru;


};
int main(void)
{
    std::vector<std::thread> threads;
    auto thr_count = std::thread::hardware_concurrency();
    boost::asio::io_context io_context;
    LRU lru(2); // size of 2
    Server srv(io_context, 15002, &lru);
    srv.async_accept();
    for(int i = 0; i < thr_count; ++i)
    {
        threads.emplace_back([&]
        {
            io_context.run();
        });
    }
    for(auto& thread : threads)
    {
        if(thread.joinable())
        {
            thread.join();
        }
    }
    return 0;
}