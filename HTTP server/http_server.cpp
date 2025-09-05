#include "http_server.hpp"


int main(){

    if (server_setup() != 0) {
        std::cerr << "Error al iniciar servidor\n";
        return 1;
    }

    //Accept a client connection
    sockaddr_in client_address;
    int address_size = sizeof(client_address);

    client_socket = accept(listening_socket, (sockaddr*)&client_address, &address_size);

    //Once a client is accepted, listen socket is no longer needed
    closesocket(listening_socket);
    if(client_socket != INVALID_SOCKET)
    {
        client_handle();
    }
    else
    {
        std::cerr << "Accept failed.\n";
        return 1;
    }

    //cleanup
    WSACleanup();
    std::cout <<"Server shutting down.\n";
    return 0;
}

int server_setup(){
    if(WSAStartup(MAKEWORD(2,2), &wsadata)!= 0)
    {
        std::cerr << "WSAStartup failed.\n";
        return 1;
    }
    std::cout << "Winsock initialized.\n";

    //Initialize listening socket
    listening_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if(listening_socket == INVALID_SOCKET)
    {
        std::cerr << "Socket creation failed.\n";
        WSACleanup();
        return 1;
    }

    //Bind listening socket
    sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);

    inet_pton(AF_INET,"127.0.0.1", &server_address.sin_addr);

    if(bind(listening_socket, (sockaddr*)&server_address, sizeof(server_address)) == SOCKET_ERROR)
    {
        std::cerr << "Bind failed." << "\n";
        closesocket(listening_socket);
        WSACleanup();
        return 1;
    }

    //Listen on socket
    if(listen(listening_socket, SOMAXCONN) == SOCKET_ERROR)
    {
        std::cerr << "Listen failed.\n";
        closesocket(listening_socket);
        WSACleanup();
        return 1;
    }

    std::cout << "Server listening on port " << PORT << "\n";
    return 0;
}

void client_handle(){
    char host[NI_MAXHOST];
    char service[NI_MAXSERV];

    char buffer[MAX_BUFFER_SIZE];
    int bytes_received;

    const char* message = "HTTP/1.1 200 OK\r\nContent-Type: text/plain\r\n\r\nMamaguevo!";

    if(getnameinfo((sockaddr*)&client_socket, sizeof(client_socket), host, NI_MAXHOST, service, NI_MAXSERV,0) == 0)
    {
        std::cout << "Accept connection from " << host << "on port " << service << "\n";
    }

    // Recibe los datos de la solicitud del cliente
    bytes_received = recv(client_socket, buffer, MAX_BUFFER_SIZE, 0);

    if(bytes_received > 0)
    {
        std::cout << "Received " << bytes_received << " bytes from client.\n";
        buffer[bytes_received] = '\0';


        std::cout << buffer<< "\n";

        send(client_socket, message, strlen(message),0);
    }

    closesocket(client_socket);
}
