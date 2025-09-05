cmake_minimum_required(VERSION 3.10)
project(sockets)

set(CMAKE_CXX_STANDARD 17)

add_executable(sockets tcp_server.cpp)

#add_executable(sockets2 udp_client.cpp)


# Enlazar la librería de Winsock
target_link_libraries(sockets ws2_32)
#target_link_libraries(sockets2 ws2_32)
