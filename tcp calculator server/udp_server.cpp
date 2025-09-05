#include <iostream>
#include <string>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

int main() {
    const int BUF_SIZE = 1024; // UDP packets are typically smaller
    const u_short PORT = 54000;

    // 1. Initialize Winsock
    WSADATA wsData;
    if (WSAStartup(MAKEWORD(2, 2), &wsData) != 0) {
        std::cerr << "WSAStartup failed." << std::endl;
        return 1;
    }
    std::cout << "Winsock initialized." << std::endl;

    // 2. Create a socket
    // CHANGE: Use SOCK_DGRAM for UDP instead of SOCK_STREAM for TCP
    SOCKET server_socket = socket(AF_INET, SOCK_DGRAM, 0);
    if (server_socket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 1;
    }

    // 3. Bind the socket to an IP address and port
    sockaddr_in serverHint;
    serverHint.sin_family = AF_INET;
    serverHint.sin_port = htons(PORT);
    serverHint.sin_addr.S_un.S_addr = INADDR_ANY; // Listen on any address

    if (bind(server_socket, (sockaddr*)&serverHint, sizeof(serverHint)) == SOCKET_ERROR) {
        std::cerr << "Bind failed: " << WSAGetLastError() << std::endl;
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }

    std::cout << "UDP Server listening on port " << PORT << "..." << std::endl;

    // --- We NO LONGER need listen() or accept() ---
    // These functions are for connection-oriented protocols like TCP.

    char buf[BUF_SIZE];
    sockaddr_in clientInfo; // This will hold the client's address info
    int clientInfoLen = sizeof(clientInfo);

    // 4. Main server loop to receive and echo packets
    while (true) {
        ZeroMemory(&clientInfo, clientInfoLen);
        ZeroMemory(buf, BUF_SIZE);

        // CHANGE: Use recvfrom() instead of recv()
        // recvfrom() will block until a packet is received. It fills in 'clientInfo'
        // with the address of the client that sent the packet.
        int bytesIn = recvfrom(server_socket, buf, BUF_SIZE, 0, (sockaddr*)&clientInfo, &clientInfoLen);
        if (bytesIn == SOCKET_ERROR) {
            std::cerr << "recvfrom error: " << WSAGetLastError() << std::endl;
            continue; // Continue to the next iteration
        }

        // Display the message and where it came from
        char clientIp[256]; // Buffer to hold the client's IP address
        ZeroMemory(clientIp, 256);

        // Convert the client's IP address from binary to a string
        inet_ntop(AF_INET, &clientInfo.sin_addr, clientIp, 256);

        std::cout << "Received packet from " << clientIp << ":" << ntohs(clientInfo.sin_port)
                  << " - Data: " << std::string(buf, 0, bytesIn) << std::endl;

        // CHANGE: Echo the message back to the client using sendto()
        // We use the 'clientInfo' struct that recvfrom() filled out to specify
        // exactly which client to send the packet to.
        sendto(server_socket, buf, bytesIn, 0, (sockaddr*)&clientInfo, clientInfoLen);
    }

    // 5. Close socket and cleanup
    closesocket(server_socket);
    WSACleanup();

    return 0;
}