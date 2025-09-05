cmake_minimum_required(VERSION 3.10)
project(chat_server)

set(CMAKE_CXX_STANDARD 17)

add_executable(chat_server chat_server.cpp)
add_executable(chat_client chat_client.cpp)


# Enlazar la librería de Winsock
target_link_libraries(chat_server ws2_32)
target_link_libraries(chat_client ws2_32)
