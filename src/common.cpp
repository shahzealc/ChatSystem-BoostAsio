#pragma once

#include <boost/asio.hpp>
#include <boost/bind/bind.hpp>
#include <iostream>
#include <string>
#include <memory>
#include <vector>
#include <thread>
#include <mutex>
#include <queue>
#include <atomic>

using boost::asio::ip::tcp;

// Message structure for communication
struct Message {
    static constexpr size_t HEADER_SIZE = 4;
    static constexpr size_t MAX_BODY_SIZE = 512;
    
    char data[HEADER_SIZE + MAX_BODY_SIZE];
    size_t body_length = 0;
    
    // Get pointer to body
    const char* body() const { return data + HEADER_SIZE; }
    char* body() { return data + HEADER_SIZE; }
    
    // Get total message length
    size_t length() const { return HEADER_SIZE + body_length; }
    
    // Encode header with body length
    void encode_header() {
        char header[HEADER_SIZE + 1] = "";
        std::sprintf(header, "%4d", static_cast<int>(body_length));
        std::memcpy(data, header, HEADER_SIZE);
    }
    
    // Decode header to get body length
    bool decode_header() {
        char header[HEADER_SIZE + 1] = "";
        std::strncat(header, data, HEADER_SIZE);
        body_length = std::atoi(header);
        return body_length <= MAX_BODY_SIZE;
    }
};

// Constants
constexpr unsigned short DEFAULT_PORT = 8080;
constexpr const char* DEFAULT_HOST = "127.0.0.1";