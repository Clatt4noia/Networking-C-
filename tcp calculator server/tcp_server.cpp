#include <iostream>
#include <string> // Used for easier string handling
#include <sstream>
#include <unordered_map>
#include <functional>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment (lib, "Ws2_32.lib")

// Define constants for configuration
const u_short PORT = 54000;
const int BUFFER_SIZE = 4096;

//struct contaning the values of the message received
struct operation_format{
    float n1 = -1;
    char opt = '\0';
    float n2 = -1;
    operation_format(float a, char c, float b)
        : n1(a), opt(c), n2(b) {}
};

//Check for valid operators
bool is_valid_operator(char op) {
    return op == '+' || op == '-' || op == '*' || op == '/';
}

/*
 * Parse the message received
 * check for a valid format for operation
 */
bool parse_operation(const char* buffer, operation_format& output) {
    // Convert buffer to a C++ string for easier manipulation
    std::string line(buffer);

    // Trim leading and trailing whitespace (spaces, \n, \r, \t) ---
    // This cleans up the input like "  10 + 5 \r\n  " into "10 + 5"
    auto first = line.find_first_not_of(" \t\n\r");
    if (std::string::npos == first) {
        // The line was empty or contained only whitespace
        return false;
    }
    auto last = line.find_last_not_of(" \t\n\r");
    line = line.substr(first, (last - first + 1));

    // --- Step 2: Parse the cleaned string ---
    std::istringstream iss(line);
    float n1, n2;
    char opt;

    // Attempt to extract the three main components.
    if (!(iss >> n1 >> opt >> n2)) {
        // Failed to get the basic "number op number" structure.
        return false;
    }

    // --- Step 3: Ensure there is no extra data ---
    std::string temp;
    if (iss >> temp) {
        // If we can read another "word" into temp, it means there's extra
        // data (e.g., "10 + 5 hello" or a second operation). This is invalid.
        return false;
    }

    // --- Step 4: Validate the operator ---
    if (!is_valid_operator(opt)) {
        return false;
    }

    // If we passed all checks, the format is valid.
    output = operation_format(n1, opt, n2);
    return true;
}

bool parse(char* buffer, operation_format& output){
    std::istringstream iss(buffer);
    float n1,n2;
    char opt;

    if(!(iss >> n1 >> opt >> n2))
    {
        std::cerr << "Error: Invalid format. Expected <float> <char> <float>";
        return false;
    }
   
    if (!is_valid_operator(opt)) {
        std::cerr << "Error: Unsupported operator '" << opt << "'.\n";
        return false;
    }

    output = operation_format(n1,opt,n2);
    return true; 
}

/*
 * perform the operation based on the corresponding operator
 * declares a static constant map named ops
 * the map keys are the valid char opts and the values are lambda functions
 */
double evaluate(float n1, char op, float n2) {
    //define a unordered_map of lambda functions each corresponds to the valids operation
    static const std::unordered_map<char, std::function<double(float, float)>> ops = {
        {'+', [](float x, float y) { return x + y; }},
        {'-', [](float x, float y) { return x - y; }},
        {'*', [](float x, float y) { return x * y; }},
        {'/', [](float x, float y) -> double {
            //check for division by zero
            if (y == 0) throw std::runtime_error("Division by zero");
            return static_cast<double>(x) / y;
        }}
    };

    //looks up the operator in the map
    auto iterator = ops.find(op);
    //return the lambda operation given the iterator value
    return iterator->second(n1, n2);
}


/*
 * Handles all communication for a single client connection.
 * client_socket The socket connected to the client.
 */
void handle_client(SOCKET client_socket) {
    char host[NI_MAXHOST];
    char service[NI_MAXSERV];
    double result = 0;

    if (getnameinfo((sockaddr*)&client_socket, sizeof(client_socket), host, NI_MAXHOST, service, NI_MAXSERV, 0) == 0) {
        std::cout << "Accepted connection from " << host << " on port " << service << std::endl;
    }

    char buffer[BUFFER_SIZE];
    int bytesReceived;

    // Echo loop
    while ((bytesReceived = recv(client_socket, buffer, BUFFER_SIZE, 0)) > 0) {
        std::cout << "Received " << bytesReceived << " bytes from client." << std::endl;
        buffer[bytesReceived] = '\0'; // Null-terminate the received buffer

        //forced check if buffer contains newlines or whitespaces before parcing
        //specially if using terminal clients
        std::string received_str(buffer);
        if (received_str.find_first_not_of(" \t\r\n") == std::string::npos) {
            continue; // Skip the rest of the loop and wait for the next real message
        }

        //parse the message
        operation_format parsed_buffer(0,'+',0);//dummy values for input format
        if(parse_operation(buffer,parsed_buffer)){
            //evaluate the result from the parsed buffer received
            result = evaluate(parsed_buffer.n1, parsed_buffer.opt, parsed_buffer.n2);
            
            //cast the result back to the buffer
            char result_buffer[128];
            int result_len = std::snprintf(result_buffer, sizeof(result_buffer), "%.2f\r\n", result);
            
            // Echo the data back to the client
            send(client_socket, result_buffer,result_len, 0);
        }
        else
        {
            std::cout << "Invalid format received. Sending error to client." << std::endl;
            const char* error_msg = "Invalid format received!\r\n";
            send(client_socket, error_msg, strlen(error_msg), 0);
        }                 
    }

    if (bytesReceived == 0) {
        std::cout << "Client disconnected." << std::endl;
    } else {
        std::cerr << "recv failed with error." << std::endl;
    }

    // Close the client socket
    closesocket(client_socket);
    std::cout << "Connection closed." << std::endl;
}

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
    std::cout << "Server listening on port " << PORT << "..." << std::endl;

    // ---- 5. Accept a Client Connection ----
    // This server is simple and only accepts one client.
    sockaddr_in clientAddr;
    int clientAddrSize = sizeof(clientAddr);
    SOCKET client_socket = accept(listening_socket, (sockaddr*)&clientAddr, &clientAddrSize);
    
    // Once a client is accepted, we no longer need the listening socket for this simple server.
    closesocket(listening_socket);

    if (client_socket != INVALID_SOCKET) {
        // ---- 6. Handle Communication with the Client ----
        handle_client(client_socket);
    } 
    else 
    {
        std::cerr << "Accept failed." << std::endl;
    }

    // ---- 7. Cleanup ----
    WSACleanup();
    std::cout << "Server shutting down." << std::endl;
    return 0;
}
