#include "common.hpp"
#include <set>

class ChatParticipant {
public:
    virtual ~ChatParticipant() = default;
    virtual void deliver(const Message& msg) = 0;
};

using chat_participant_ptr = std::shared_ptr<ChatParticipant>;

class ChatRoom {
private:
    std::set<chat_participant_ptr> participants_;
    std::queue<Message> recent_messages_;
    std::mutex mutex_;
    static constexpr size_t MAX_RECENT_MSGS = 100;

public:
    void join(chat_participant_ptr participant) {
        std::lock_guard<std::mutex> lock(mutex_);
        participants_.insert(participant);
        
        // Send recent messages to new participant
        for (const auto& msg : std::queue<Message>(recent_messages_)) {
            participant->deliver(msg);
        }
    }
    
    void leave(chat_participant_ptr participant) {
        std::lock_guard<std::mutex> lock(mutex_);
        participants_.erase(participant);
    }
    
    void deliver(const Message& msg) {
        std::lock_guard<std::mutex> lock(mutex_);
        
        // Add to recent messages
        recent_messages_.push(msg);
        while (recent_messages_.size() > MAX_RECENT_MSGS) {
            recent_messages_.pop();
        }
        
        // Deliver to all participants
        for (auto participant : participants_) {
            participant->deliver(msg);
        }
    }
};

class ChatSession : public ChatParticipant, 
                   public std::enable_shared_from_this<ChatSession> {
private:
    tcp::socket socket_;
    ChatRoom& room_;
    Message read_msg_;
    std::queue<Message> write_msgs_;
    std::mutex write_mutex_;
    std::atomic<bool> writing_{false};

public:
    ChatSession(tcp::socket socket, ChatRoom& room)
        : socket_(std::move(socket)), room_(room) {}
    
    void start() {
        room_.join(shared_from_this());
        do_read_header();
    }
    
    void deliver(const Message& msg) override {
        bool write_in_progress = writing_.exchange(true);
        
        {
            std::lock_guard<std::mutex> lock(write_mutex_);
            write_msgs_.push(msg);
        }
        
        if (!write_in_progress) {
            do_write();
        }
    }

private:
    void do_read_header() {
        auto self(shared_from_this());
        boost::asio::async_read(socket_,
            boost::asio::buffer(read_msg_.data, Message::HEADER_SIZE),
            [this, self](boost::system::error_code ec, std::size_t /*length*/) {
                if (!ec && read_msg_.decode_header()) {
                    do_read_body();
                } else {
                    room_.leave(shared_from_this());
                }
            });
    }
    
    void do_read_body() {
        auto self(shared_from_this());
        boost::asio::async_read(socket_,
            boost::asio::buffer(read_msg_.body(), read_msg_.body_length),
            [this, self](boost::system::error_code ec, std::size_t /*length*/) {
                if (!ec) {
                    // Add timestamp and client info
                    auto now = std::time(nullptr);
                    std::string timestamp = std::ctime(&now);
                    timestamp.pop_back(); // Remove newline
                    
                    std::string formatted_msg = "[" + timestamp + "] Client: " + 
                                              std::string(read_msg_.body(), read_msg_.body_length);
                    
                    Message response;
                    response.body_length = std::min(formatted_msg.size(), 
                                                  static_cast<size_t>(Message::MAX_BODY_SIZE));
                    std::memcpy(response.body(), formatted_msg.c_str(), response.body_length);
                    response.encode_header();
                    
                    room_.deliver(response);
                    do_read_header();
                } else {
                    room_.leave(shared_from_this());
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
        
        auto self(shared_from_this());
        boost::asio::async_write(socket_,
            boost::asio::buffer(msg.data, msg.length()),
            [this, self](boost::system::error_code ec, std::size_t /*length*/) {
                if (!ec) {
                    do_write();
                } else {
                    room_.leave(shared_from_this());
                    writing_ = false;
                }
            });
    }
};

class ChatServer {
private:
    tcp::acceptor acceptor_;
    ChatRoom room_;

public:
    ChatServer(boost::asio::io_context& io_context, const tcp::endpoint& endpoint)
        : acceptor_(io_context, endpoint) {
        do_accept();
    }

private:
    void do_accept() {
        acceptor_.async_accept(
            [this](boost::system::error_code ec, tcp::socket socket) {
                if (!ec) {
                    std::cout << "New client connected from: " 
                             << socket.remote_endpoint() << std::endl;
                    
                    std::make_shared<ChatSession>(std::move(socket), room_)->start();
                }
                do_accept();
            });
    }
};

int main(int argc, char* argv[]) {
    try {
        unsigned short port = DEFAULT_PORT;
        if (argc > 1) {
            port = std::atoi(argv[1]);
        }
        
        boost::asio::io_context io_context;
        tcp::endpoint endpoint(tcp::v4(), port);
        ChatServer server(io_context, endpoint);
        
        std::cout << "Chat Server starting on port " << port << "..." << std::endl;
        std::cout << "Press Ctrl+C to stop the server." << std::endl;
        
        // Run io_context in multiple threads for better performance
        std::vector<std::thread> threads;
        const size_t thread_count = std::thread::hardware_concurrency();
        
        for (size_t i = 0; i < thread_count; ++i) {
            threads.emplace_back([&io_context]() {
                io_context.run();
            });
        }
        
        for (auto& t : threads) {
            t.join();
        }
    }
    catch (std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}