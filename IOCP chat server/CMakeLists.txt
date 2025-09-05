cmake_minimum_required(VERSION 3.10)
project(iocp_server)

set(CMAKE_CXX_STANDARD 17)

add_executable(iocp_server iocp_server.cpp)
add_executable(iocp_client iocp_client.cpp)


# Enlazar la librería de Winsock
target_link_libraries(iocp_server ws2_32)
target_link_libraries(iocp_server mswsock)
target_link_libraries(iocp_client ws2_32)
