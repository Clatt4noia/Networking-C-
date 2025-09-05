#include <iostream>
#include <string>
#include <thread>
#include <mutex>
#include <atomic>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>
#include <conio.h>

#pragma comment(lib, "ws2_32.lib")

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 5000

std::string current_input = "";// Store users current prompt
std::mutex console_mutex;//Synchronize terminal access
std::atomic<bool> username_entered(false);

// Activate ANSI support
void enable_virtual_terminal_processing() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
}

//Update current prompt 
void redraw_prompt(){
    std::cout << "\r\x1b[2K" //ANSI code to move prompt to terminal bottom
              << "You:" <<current_input << std::flush;
}

void receiveMessages(SOCKET sock) {
    char buffer[4096];
    while (true) {
        ZeroMemory(buffer, 4096);
        int bytes_received = recv(sock, buffer, sizeof(buffer), 0);
        if (bytes_received > 0) {
            std::string received_msg(buffer, 0, bytes_received);

            //Block mutex to prevent from user writing corruption
            std::lock_guard<std::mutex> guard(console_mutex);

            // Move cursor to start line and delete current prompt
            std::cout << "\r\x1b[2K";

            //print received message
            std::cout << received_msg << std::endl;

            //redraw prompt at the bottom
            // Only redraw prompt if moved past the username phase
            if (username_entered.load()) {
                redraw_prompt();
            }
            // if username not entered, do NOT redraw (user is expected to type username)

        } else {
            std::cout << "\nDisconnected from server.\n";
            break;
        }
    }
}

int main() {

    enable_virtual_terminal_processing();

    // ----  Initialize Winsock ----
    WSADATA wsData;
    if (WSAStartup(MAKEWORD(2, 2), &wsData) != 0) {
        std::cerr << "WSAStartup failed.\n";
        return 1;
    }

    // ---- Create Socket ----
    SOCKET client_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (client_socket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed.\n";
        WSACleanup();
        return 1;
    }

    // ---- Server Address ----
    sockaddr_in serverAddr;
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(SERVER_PORT);
    inet_pton(AF_INET, SERVER_IP, &serverAddr.sin_addr);

    // ---- Connect to Server ----
    if (connect(client_socket, (sockaddr*)&serverAddr, sizeof(serverAddr)) == SOCKET_ERROR) {
        std::cerr << "Could not connect to server.\n";
        closesocket(client_socket);
        WSACleanup();
        return 1;
    }

    /*---- Start receiving messages ----
    *  receives messages loop in a new thread
    */ 
    std::thread receiver_thread(receiveMessages,client_socket);
    //allow receiver_thread to work in background
    receiver_thread.detach();

    // ---- 6. First input: username ----
    std::string username;
    std::getline(std::cin, username);
    int sent = send(client_socket, username.c_str(), username.size(), 0);
    if (sent == SOCKET_ERROR) {
        std::cerr << "Failed to send username.\n";
        return 1;
    } else {
        // Mark that client is now in chat phase and allow redraws
        username_entered.store(true);

        // Draw the chat prompt now that username is set
        {
            std::lock_guard<std::mutex> guard(console_mutex);
            redraw_prompt();
        }
    }

    // ---- 7. Send chat messages ----
    while (true) {
        char c = _getch(); // read input

        std::lock_guard<std::mutex> guard(console_mutex);

        if (c == '\r') { // Enter key
            if (!current_input.empty()) {
                send(client_socket, current_input.c_str(), current_input.size(), 0);
                std::cout << std::endl; // move to newline
                current_input.clear();
            }
        } else if (c == '\b') { // Backspace key
            if (!current_input.empty()) {
                current_input.pop_back();
            }
        } else if (isprint(c)) { // Every other char
            current_input += c;
        }

        redraw_prompt(); // Redraw new prompt
    }

    // ---- 8. Cleanup ----
    shutdown(client_socket, SD_BOTH);
    closesocket(client_socket);
    WSACleanup();

    std::cout << "Client exited.\n";
    return 0;
}