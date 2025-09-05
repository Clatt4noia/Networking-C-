#define NOMINMAX
#include <iostream>
#include <string>
#include <WinSock2.h>
#include <WS2tcpip.h>
#include <mswsock.h>
#include <unordered_map>
#include <thread>
#include <atomic>
#include <algorithm>

#define OPERATION_KEY 1
#define PORT 5000
#define MAX_BUFFER_SIZE 4096

//GLOBAL IOCP HANDLE
HANDLE iocp_handle = INVALID_HANDLE_VALUE;

//Global listening socket
//SOCKET listening_socket = INVALID_SOCKET;

//operation type
enum operation_type{accept_op, read_op, write_op, welcome_op, username_op};

//Socket context struct to handle clients information
struct client_context{
    SOCKET client_socket;
    std::string username;
};

// Context used for every operation type
struct io_context {
    WSAOVERLAPPED overlapped;     // always needed
    operation_type op_type;       // accept / recv / send
    client_context* parent;       // the client context (nullptr for accept)

    // Buffers for recv/send
    WSABUF wsabuf;
    char buffer[MAX_BUFFER_SIZE];

    // AcceptEx specific
    SOCKET accept_socket;
    char accept_buffer[(sizeof(sockaddr_in) + 16) * 2];
};

//GLOBAL UNORDERED_MAP OF CONTEXT STRUCTURES
std::unordered_map <SOCKET, client_context*> clients;

//Shared bool between threads
//std::atomic<bool> running = true;

/*Create and initialize client context info
* Asociate client context to IOCP handle
* Add client context to clients map
*/
client_context* create_client(io_context* io_ctx, SOCKET listening_socket)
{

    client_context* client_ctx = new client_context;
    client_ctx->client_socket = io_ctx->accept_socket;
    client_ctx->username = "";
    io_ctx->parent = client_ctx;

    setsockopt(io_ctx->accept_socket,
           SOL_SOCKET,
           SO_UPDATE_ACCEPT_CONTEXT,
           (char*)&listening_socket,
           sizeof(listening_socket));

    CreateIoCompletionPort((HANDLE)client_ctx->client_socket, iocp_handle, (ULONG_PTR)client_ctx, 0);
    if(iocp_handle == nullptr)
    {
        std::cerr << "CreateCompletionPort() failed at: "<< GetLastError() <<"\n";
        closesocket(io_ctx->accept_socket);
        delete client_ctx;
        return nullptr;
    }
    
    clients[io_ctx->accept_socket] = client_ctx;

    return client_ctx;
}


/*Post an accept operation into IOCP
*/
void post_accept(SOCKET listening_socket){
    io_context* io_ctx = new io_context{};
    ZeroMemory(&io_ctx->overlapped, sizeof(io_ctx->overlapped));
    io_ctx->accept_socket = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
    io_ctx->op_type = accept_op;
    io_ctx->parent = nullptr;

    DWORD bytes = 0;
    BOOL result = AcceptEx(
        listening_socket, 
        io_ctx->accept_socket, 
        io_ctx->accept_buffer, 0, 
        sizeof(sockaddr_in)+ 16, 
        sizeof(sockaddr_in) +16, 
        &bytes,
        &io_ctx->overlapped
    );

    if(!result && WSAGetLastError() != WSA_IO_PENDING)
    {
        std::cerr << "AcceptEx() failed: " << WSAGetLastError()  << "\n";
        closesocket(io_ctx->accept_socket);
        delete io_ctx;
    }
}

/*Post a write operation into IOCP
*/
void post_send(client_context* client_ctx, std::string msg, operation_type op)
{
    io_context* ctx = new io_context{};

    ZeroMemory(&ctx->overlapped,sizeof(ctx->overlapped));
    
    size_t copy_len = std::min(msg.size(), (size_t)MAX_BUFFER_SIZE - 1);
    memcpy(ctx->buffer, msg.data(), copy_len);

    // null-terminate to be safe if treating it as a string
    ctx->buffer[copy_len] = '\0';
    ctx->wsabuf.buf = ctx->buffer;
    ctx->wsabuf.len = (ULONG)copy_len;
    ctx->op_type = op;
    ctx->parent = client_ctx;

    int result = WSASend(client_ctx->client_socket, &ctx->wsabuf,1,nullptr,0, &ctx->overlapped, nullptr);
    if(result == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING){
        std::cerr << "Error at WSASend: " << WSAGetLastError() << "\n";
        delete ctx;
    } 
};

/*Post a read operation into IOCP
*/
void post_read(client_context* client_ctx, operation_type op)
{
    io_context* ctx = new io_context;
    ZeroMemory(&ctx->overlapped,sizeof(ctx->overlapped));
    ctx->wsabuf.buf = ctx->buffer;
    ctx->wsabuf.len = MAX_BUFFER_SIZE;
    ctx->op_type = op;
    ctx->parent = client_ctx;

    DWORD flags = 0;
    int result = WSARecv(client_ctx->client_socket, &ctx->wsabuf, 1, nullptr, &flags, &ctx->overlapped, nullptr);
    if(result == SOCKET_ERROR && WSAGetLastError() != WSA_IO_PENDING){
        std::cerr << "Error at WSARecv: " << WSAGetLastError() << "\n";
        delete ctx;
    }

}

/*
*/
void event_loop(SOCKET listening_socket)
{
    while(true)
    {
        DWORD bytes_transferred;
        ULONG_PTR key; //completion key
        io_context* io_ctx;
        client_context* client_ctx;
        LPOVERLAPPED overlapped;

        BOOL status = GetQueuedCompletionStatus(iocp_handle, &bytes_transferred, &key, &overlapped, INFINITE);

        if (overlapped == nullptr) {
            // Serious IOCP error (or shutdown wake), decide what to do (log/continue/return)
            std::cerr << "GetQueuedCompletionStatus failed: " << GetLastError() << "\n";
            continue;
        }

        /*recover client context from key to know which client the operation belongs to.
        recover io operation context from overlapped to access op information(type, buffer, etc)
        */
        client_ctx = reinterpret_cast<client_context*>(key);
        io_ctx     = CONTAINING_RECORD(overlapped, io_context, overlapped);

        // If the op failed, handle it
        if (!status) 
        {
            DWORD err = GetLastError();
            if (io_ctx->op_type == accept_op) {
                // Accept failed: clean up and re-post another accept
                if (io_ctx->accept_socket != INVALID_SOCKET) closesocket(io_ctx->accept_socket);
                delete io_ctx;
                post_accept(listening_socket);
                continue;
            } else {
                // recv/send failed: drop the client
                if (client_ctx) {
                    closesocket(client_ctx->client_socket);
                    clients.erase(client_ctx->client_socket);
                    delete client_ctx;
                }
                delete io_ctx;
                continue;
            }
        }

        switch(io_ctx->op_type)
        {
            //
            case accept_op:
            {
                client_context* client_ctx = create_client(io_ctx, listening_socket);
                if(client_ctx == nullptr){
                    std::cerr <<"Client context creation failed!\n";
                    delete io_ctx;
                    post_accept(listening_socket);
                    break;
                }

                //Initial message for username prompt
                std::string welcome_msg = "Welcome to Chat Server!\r\nPlease enter an username:";

                post_send(client_ctx, welcome_msg, welcome_op);

                //cleanup the accept operation
                delete(io_ctx);

                // queue another accept flowing
                post_accept(listening_socket);

                break;
            }
            //
            case welcome_op:
                post_read(client_ctx,username_op);
                delete(io_ctx);
                break; 
            //
            case username_op:
            {
                //client closed before sending username
                if(bytes_transferred <= 0)
                {
                    //Drop client
                    closesocket(client_ctx->client_socket);
                    clients.erase(client_ctx->client_socket);
                    delete(client_ctx);
                    delete(io_ctx);
                    break;
                }
                //Add username to client context
                std::string username(io_ctx->buffer, bytes_transferred);
                username.erase(std::remove(username.begin(), username.end(), '\r'), username.end());
                username.erase(std::remove(username.begin(), username.end(), '\n'), username.end());

                client_ctx->username = username;
                //Notify new connection
                std::cout <<  username + " has joined the chat!\r\n";
                
                //post normal messages read operation mode
                post_read(client_ctx, read_op);
                delete io_ctx;
                break;
            }
            //
            case read_op:
            {
                //client closed
                if(bytes_transferred <= 0)
                {
                    //Drop client
                    closesocket(client_ctx->client_socket);
                    clients.erase(client_ctx->client_socket);
                    delete(client_ctx);
                    delete(io_ctx);
                    break;
                }

                //Append username before message --> username: "message"
                std::string message = client_ctx->username + ":" + std::string(io_ctx->buffer, bytes_transferred);

                //Broadcast messages to other clients
                for(auto&[sock, aux_ctx] : clients)
                {
                    if(sock != client_ctx->client_socket)
                    {
                        post_send(aux_ctx, message, write_op);
                    }
                }
                post_read(client_ctx, read_op);
                delete io_ctx;
                break;
            }
            //
            case write_op:
                post_read(client_ctx, read_op);
                delete io_ctx;
                break; 
        }

    }

}


int main(){

    //Initialize Winsock
    WSADATA wsData;
    if(WSAStartup(MAKEWORD(2,2), &wsData)!= 0){
        std::cerr << "WSAStartup failed." << "\n";
        return 1;
    }
    //Create listening socket
    SOCKET listening_socket = WSASocketW(AF_INET, SOCK_STREAM, IPPROTO_TCP, nullptr, 0, WSA_FLAG_OVERLAPPED);
    if (listening_socket == INVALID_SOCKET) {
        std::cerr << "Listening socket creation failed. " << WSAGetLastError() << "\n";
        WSACleanup();
        return 1;
    }

    //Socket binding
    sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(PORT);
    inet_pton(AF_INET, "127.0.0.1", &server_address.sin_addr);

    if(bind(listening_socket, (sockaddr*)&server_address, sizeof(server_address)) == SOCKET_ERROR){
        std::cerr << "Bind/connect failed." << "\n";
        closesocket(listening_socket);
        WSACleanup();
        return 1;
    }

     // listen on the socket ----
    if (listen(listening_socket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Listen failed." << std::endl;
        closesocket(listening_socket);
        WSACleanup();
        return 1;
    }
    std::cout << "Welcome to Chat Room Server!" << std::endl;
    std::cout << "Server listening on port " << PORT << "..." << std::endl;

    //Create a I/O completion port
    iocp_handle = CreateIoCompletionPort(INVALID_HANDLE_VALUE, nullptr, 0, 0);

    //Associate the socket handle to IOCP
    CreateIoCompletionPort((HANDLE)listening_socket, iocp_handle, 0, 0);

    /*Initial accepting operation post into IOCP
    *from here on, accept_op completions will handle new connections
    */
    post_accept(listening_socket);

    //Create event thread
    std::thread event_thread(event_loop, listening_socket);
    
    event_thread.join();

    WSACleanup();
    return 0;
}
