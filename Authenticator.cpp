#include "MemAlloc.cpp"
#include <boost/asio.hpp>
#include <optional>
#include <iostream>
#include <unordered_set>
class AuthenticatorServer {

};

class session : public std::enable_shared_from_this<session>
{
    public:
        session(boost::asio::ip::tcp::socket&& socket)
        : socket(std::move(socket)), streambuf(512)
        {
            allowed_users.push_back("abc:def");
            allowed_users.push_back("user:password");
            allowed_users.push_back("xyz:def");
        }

        void read_req()
        {
            boost::asio::async_read_until(socket, streambuf, ';', [self = shared_from_this()] (boost::system::error_code error, std::size_t bytes_transferred)
            {
                bool ok_or_not = false;
                if(!error)
                    ok_or_not = self->check_req(const_cast<char *>(boost::asio::buffer_cast<const char*>(self->streambuf.data())), bytes_transferred);
                self->write_back(ok_or_not);
            });
        }

        void write_back(bool ok_or_not)
        {
            if(ok_or_not) // send ok
            {
                boost::asio::async_write(socket, boost::asio::buffer("ok",2), [self = shared_from_this()] (boost::system::error_code error, std::size_t bytes_transferred)
                {
                    if(error)
                        std::cerr << error.message() << std::endl;
                    self->read_req();
                });
            } else {
                boost::asio::async_write(socket, boost::asio::buffer("not_ok",6), [self = shared_from_this()] (boost::system::error_code error, std::size_t bytes_transferred)
                {
                    if(error)
                        std::cerr << error.message() << std::endl;
                    self->read_req();
                });
            }
        }

        bool check_req(char* buffer, size_t bytes) {
            auto creds = strtok(buffer, ";");
            if(!creds)
            {
                return false;
            }
            for(const auto &i : allowed_users)
            {
                if(!strcmp(creds,i.c_str()))
                {
                    streambuf.consume(512);
                    return true;
                }
            }
            streambuf.consume(512);
            return false;
        }
    private:
        boost::asio::ip::tcp::socket socket;
        boost::asio::streambuf streambuf;
        std::vector<std::string> allowed_users;
};

class server
{
    public:
    
        server(boost::asio::io_context& io_context, std::uint16_t port)
        : io_context(io_context)
        , acceptor  (io_context, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port))
        {
        }
    
        void async_accept()
        {
            socket.emplace(io_context);
    
            acceptor.async_accept(socket.value(), [&] (boost::system::error_code error)
            {
                std::make_shared<session>(std::move(*socket))->read_req();
                async_accept();
            });
        }
    
    private:
    
        boost::asio::io_context& io_context;
        boost::asio::ip::tcp::acceptor acceptor;
        std::optional<boost::asio::ip::tcp::socket> socket;
};
int main(void)
{
    std::vector<std::thread> threads;
    auto thr_count = std::thread::hardware_concurrency() * 2;
    boost::asio::io_context io_context;
    server srv(io_context, 15001);
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