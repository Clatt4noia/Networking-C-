#include <iostream>
#include <string>
#include <WinSock2.h>
#include <WS2tcpip.h>

int main(){

    //Initialize Winsock
    WSADATA wsadata;
    WSAStartup(MAKEWORD(2,2), &wsadata);

    //create a UDP socket
    SOCKET client_socket = socket(AF_INET, SOCK_DGRAM, 0);

    // Create a struct for the server address
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(54000); // Use the same port as the server

    // Convert the IP address string to the required format
    // inet_pton is the modern function for this.
    inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr);

    //stablish the message
    std::string userInput;
    std::cout << "Enter a message to send to the server: ";
    std::getline(std::cin,userInput); //for input with spaces

    // Send the message
    sendto(client_socket, 
       userInput.c_str(),            // The data to send
       userInput.length(),           // The length of the data
       0,                            // Flags
       (sockaddr*)&serverAddr,       // The destination address
       sizeof(serverAddr));
    

    //receiving the echoed message
    char buf[1024];
    ZeroMemory(buf, 1024);

    // Wait for the response
    int bytesReceived = recvfrom(client_socket, 
                             buf, 
                             1024, 
                             0, 
                             nullptr, // Don't care about sender's address
                             nullptr);

    // Print the response
    if (bytesReceived > 0) {
        std::cout << "SERVER ECHO> " << std::string(buf, 0, bytesReceived) << std::endl;
    }
    
    //close the socket and cleanup Winsock
    closesocket(client_socket);
    WSACleanup();
    
    return 0;
}