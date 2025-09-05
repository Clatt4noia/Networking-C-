#ifndef httpserver_hpp
#define httpserver_hpp

#include <iostream>
#include <string>
#include <WinSock2.h>
#include <WS2tcpip.h>

/*
*/
#define PORT  8080
#define MAX_BUFFER_SIZE  1024

WSADATA wsadata; 
SOCKET listening_socket = INVALID_SOCKET; 
SOCKET client_socket = INVALID_SOCKET;

int server_setup();

void client_handle();


#endif
