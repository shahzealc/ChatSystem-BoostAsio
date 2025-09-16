cmake_minimum_required(VERSION 3.12)
project(ChatSystem)

# Set C++ standard
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Boost
find_package(Boost REQUIRED COMPONENTS system)

# Include directories
include_directories(${Boost_INCLUDE_DIRS})

# Compiler flags for better debugging and warnings
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -O2")

# Create server executable
add_executable(chat_server src/server.cpp)
target_link_libraries(chat_server ${Boost_LIBRARIES})

# Create client executable  
add_executable(chat_client src/client.cpp)
target_link_libraries(chat_client ${Boost_LIBRARIES})

# Platform-specific linking
if(WIN32)
    target_link_libraries(chat_server ws2_32 wsock32)
    target_link_libraries(chat_client ws2_32 wsock32)
elseif(UNIX)
    target_link_libraries(chat_server pthread)
    target_link_libraries(chat_client pthread)
endif()