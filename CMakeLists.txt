cmake_minimum_required(VERSION 3.16)
project(P2PChat)

set(CMAKE_AUTOMOC ON) 
find_package(Qt6 COMPONENTS Widgets Network REQUIRED)

add_executable(Chatbox main.cpp udp.cpp)
target_link_libraries(Chatbox Qt6::Widgets Qt6::Network)

