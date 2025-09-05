#include <iostream>
#include <string> 
#include <unordered_map>
#include <functional>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment (lib, "Ws2_32.lib")

// Define constants for configuration
const u_short PORT = 54000;
const int BUFFER_SIZE = 4096;

int main() {
    // ---- 1. Initialize Winsock ----
    WSADATA wsData;
    if (WSAStartup(MAKEWORD(2, 2), &wsData) != 0) {
        std::cerr << "WSAStartup failed." << std::endl;
        return 1;
    }
    std::cout << "Winsock initialized." << std::endl;

    // ---- 2. Create Listening Socket ----
    SOCKET listening_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (listening_socket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed." << std::endl;
        WSACleanup();
        return 1;
    }

    // ---- 3. Bind the Socket ----
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(PORT);
    //serverAddr.sin_addr.S_un.S_addr = INADDR_ANY; <----Bind the socket to all network interfaces available
    inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr); //restrict binding to loopback address

    if (bind(listening_socket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Bind failed." << std::endl;
        closesocket(listening_socket);
        WSACleanup();
        return 1;
    }

    // ---- 4. Listen on the Socket ----
    if (listen(listening_socket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Listen failed." << std::endl;
        closesocket(listening_socket);
        WSACleanup();
        return 1;
    }
    std::cout << "Welcome to Chat Room Server!" << std::endl;
    std::cout << "Server listening on port " << PORT << "..." << std::endl;

    /*using Select() and FD functions for multiple clients management 
    */
    
    // Create a set of sockets and clear it 
    fd_set master;
    FD_ZERO(&master);

    //Add listening socket to the set
    FD_SET(listening_socket, &master);

    //Connected users management
    std::unordered_map<SOCKET, std::string> users;

    while(true)
    {
        //create a copy of master set
        fd_set aux = master;

        int socket_count = select(0, &aux, nullptr, nullptr, nullptr);

        for(int i = 0; i < socket_count; i++ ){
            //
            SOCKET current_socket = aux.fd_array[i];
            if(current_socket == listening_socket){
                //Accept a new connection
                SOCKET new_client = accept(listening_socket,nullptr,nullptr);
                //Add new connection to the list
                FD_SET(new_client, &master);
                //send a welcome message to new client
                std::string welcome_msg = "Welcome to Chat Server!\r\nPlease enter an username:";
                send(new_client, welcome_msg.c_str(), welcome_msg.size(), 0); 
        
            }else{
                //Accept a new message
                char buffer[BUFFER_SIZE];
                ZeroMemory(buffer, BUFFER_SIZE);

                int bytes_received = recv(current_socket, buffer, BUFFER_SIZE, 0);
                if(bytes_received <= 0){
                    if (users.find(current_socket) != users.end()) {
                        std::string left_msg = users[current_socket] + " has left the chat.\r\n";

                        // Broadcast disconnection
                        for (int j = 0; j < master.fd_count; ++j) {
                            SOCKET output_socket = master.fd_array[j];
                            if (output_socket != listening_socket && output_socket != current_socket) {
                                send(output_socket, left_msg.c_str(), left_msg.size(), 0);
                                ZeroMemory(buffer,BUFFER_SIZE);
                            }
                        }

                        users.erase(current_socket);
                    }

                    //Drop client
                    closesocket(current_socket);
                    FD_CLR(current_socket, &master); 
                }else{
                    //verify if user is already registered
                    if(users.find(current_socket) == users.end())
                    {
                        //Add new user to map
                        std::string username(buffer, bytes_received);
                        username.erase(std::remove(username.begin(), username.end(), '\r'), username.end());
                        username.erase(std::remove(username.begin(), username.end(), '\n'), username.end());

                        users[current_socket] = username;

                        //Notify new connection
                        std::cout <<  username + " has joined the chat!\r\n";
                
                    }
                    else
                    {
                        //forced check if buffer contains newlines or whitespaces before parcing
                        //specially if using terminal clients
                        std::string received_str(buffer, bytes_received);
                        if (received_str.find_first_not_of(" \t\r\n") == std::string::npos) {
                            continue; // Skip the rest of the loop and wait for the next real message
                        }
                        std::string message = users[current_socket] + ":" + std::string(buffer,bytes_received);
                        //send message to other clients
                        for(int i = 0; i< master.fd_count; i++ ){
                            SOCKET output_socket  = master.fd_array[i];
                            if(output_socket != listening_socket && output_socket != current_socket){
                                send(output_socket, message.c_str(), message.size(), 0);
                            }
                        }

                    }
                
                }
            }
        }
    }

    // ---- 7. Cleanup ----
    WSACleanup();
    std::cout << "Server shutting down." << std::endl;
    return 0;
}
