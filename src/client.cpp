#include "common.hpp"
#include <cstdlib>

class ChatClient {
private:
    boost::asio::io_context& io_context_;
    tcp::socket socket_;
    Message read_msg_;
    std::queue<Message> write_msgs_;
    std::mutex write_mutex_;
    std::atomic<bool> connected_{false};
    std::atomic<bool> writing_{false};

public:
    ChatClient(boost::asio::io_context& io_context)
        : io_context_(io_context), socket_(io_context) {}
    
    void connect(const tcp::resolver::results_type& endpoints) {
        boost::asio::async_connect(socket_, endpoints,
            [this](boost::system::error_code ec, tcp::endpoint) {
                if (!ec) {
                    connected_ = true;
                    std::cout << "\n=== Connected to Chat Server ===" << std::endl;
                    std::cout << "Type your messages and press Enter. Type 'quit' to exit." << std::endl;
                    std::cout << "=================================" << std::endl;
                    do_read_header();
                } else {
                    std::cerr << "Connection failed: " << ec.message() << std::endl;
                }
            });
    }
    
    void write(const Message& msg) {
        if (!connected_) {
            std::cerr << "Not connected to server!" << std::endl;
            return;
        }
        
        bool write_in_progress = writing_.exchange(true);
        {
            std::lock_guard<std::mutex> lock(write_mutex_);
            write_msgs_.push(msg);
        }
        
        if (!write_in_progress) {
            do_write();
        }
    }
    
    void close() {
        connected_ = false;
        boost::asio::post(io_context_, [this]() { socket_.close(); });
    }
    
    bool is_connected() const { return connected_; }

private:
    void do_read_header() {
        boost::asio::async_read(socket_,
            boost::asio::buffer(read_msg_.data, Message::HEADER_SIZE),
            [this](boost::system::error_code ec, std::size_t /*length*/) {
                if (!ec && read_msg_.decode_header()) {
                    do_read_body();
                } else {
                    connected_ = false;
                    socket_.close();
                }
            });
    }
    
    void do_read_body() {
        boost::asio::async_read(socket_,
            boost::asio::buffer(read_msg_.body(), read_msg_.body_length),
            [this](boost::system::error_code ec, std::size_t /*length*/) {
                if (!ec) {
                    std::cout << std::string(read_msg_.body(), read_msg_.body_length) 
                             << std::endl;
                    do_read_header();
                } else {
                    connected_ = false;
                    socket_.close();
                }
            });
    }
    
    void do_write() {
        Message msg;
        {
            std::lock_guard<std::mutex> lock(write_mutex_);
            if (write_msgs_.empty()) {
                writing_ = false;
                return;
            }
            msg = write_msgs_.front();
            write_msgs_.pop();
        }
        
        boost::asio::async_write(socket_,
            boost::asio::buffer(msg.data, msg.length()),
            [this](boost::system::error_code ec, std::size_t /*length*/) {
                if (!ec) {
                    do_write();
                } else {
                    connected_ = false;
                    socket_.close();
                    writing_ = false;
                }
            });
    }
};

int main(int argc, char* argv[]) {
    try {
        std::string host = DEFAULT_HOST;
        std::string port = std::to_string(DEFAULT_PORT);
        
        if (argc > 1) host = argv[1];
        if (argc > 2) port = argv[2];
        
        boost::asio::io_context io_context;
        tcp::resolver resolver(io_context);
        auto endpoints = resolver.resolve(host, port);
        
        ChatClient client(io_context);
        client.connect(endpoints);
        
        // Run io_context in separate thread
        std::thread io_thread([&io_context]() { 
            io_context.run(); 
        });
        
        // Input handling in main thread
        std::string line;
        while (std::getline(std::cin, line)) {
            if (!client.is_connected()) {
                std::cout << "Disconnected from server. Exiting..." << std::endl;
                break;
            }
            
            if (line == "quit" || line == "exit") {
                break;
            }
            
            if (line.empty()) continue;
            
            Message msg;
            msg.body_length = std::min(line.length(), 
                                     static_cast<size_t>(Message::MAX_BODY_SIZE));
            std::memcpy(msg.body(), line.c_str(), msg.body_length);
            msg.encode_header();
            
            client.write(msg);
        }
        
        client.close();
        io_context.stop();
        io_thread.join();
        
    } catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}