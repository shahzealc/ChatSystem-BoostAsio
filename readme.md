# TCP Chat System with Boost.ASIO

Multi-threaded TCP chat server and client using Boost.ASIO library, demonstrating modern C++ networking and asynchronous programming.

## Features
- **Multi-threaded Server**: Handles multiple clients concurrently
- **Asynchronous I/O**: Non-blocking network operations
- **Real-time Broadcasting**: Messages delivered to all connected clients
- **Thread-safe Operations**: Mutex-protected shared resources
- **Cross-platform**: Windows, Linux, macOS support

## Tech Stack
- **C++17**: Modern C++ features, RAII, smart pointers
- **Boost.ASIO**: TCP sockets, async operations, event loops
- **Multi-threading**: `std::thread`, `std::mutex`, `std::atomic`
- **CMake**: Cross-platform build system

## Key Concepts Demonstrated
- TCP socket programming with Boost.ASIO
- Asynchronous callback-based architecture
- Thread-safe client session management
- Producer-consumer message queuing
- Modern C++ design patterns and best practices
