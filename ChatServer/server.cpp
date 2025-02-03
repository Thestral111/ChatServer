#include <winsock2.h>
#include <ws2tcpip.h>
#include <iostream>
#include <string>
#include <thread>
#include <vector>
#include <mutex>
#include <map>

#pragma comment(lib, "Ws2_32.lib")
using namespace std;



std::vector<SOCKET> clients;
std::mutex clients_mutex;

std::map<std::string, SOCKET> clientUserMap;

// send userlist to all clients
void BroadcastUserList() {
    std::string userListMessage = "#USERS ";  // Prefix to identify user list updates
    clients_mutex.lock();
    for (const auto& entry : clientUserMap) {
        userListMessage += entry.first + " ";
    }
    clients_mutex.unlock();

    // Send updated user list to all clients
    for (SOCKET client : clients) {
        send(client, userListMessage.c_str(), userListMessage.length(), 0);
    }
}

// when a new client connected, a thread is launched to run this function to handle messages
void HandleClient(SOCKET clientSocket, int id) {
    char buffer[512];
    std::cout << "thread\n";


    // Receive the username from the client

    string username = "user" + to_string(id);

    clients_mutex.lock();
    clientUserMap[username] = clientSocket;
    clients_mutex.unlock();

    std::cout << username << " has joined the chat.\n";
    BroadcastUserList();  // Send updated user list to all clients
    
    // loop to receive messages
    while (true) {
        memset(buffer, 0, sizeof(buffer));
        int bytesReceived = recv(clientSocket, buffer, sizeof(buffer), 0);
        // when a client disconnected, delete it from user list
        if (bytesReceived <= 0) {
            closesocket(clientSocket);
            clients_mutex.lock();
            std::cout << username << " disconnected.\n";
            clientUserMap.erase(username); // Remove from user list
            clients.erase(std::remove(clients.begin(), clients.end(), clientSocket), clients.end());
            clients_mutex.unlock();

            // update user list panel after a client disconnected
            BroadcastUserList();  // Send updated user list to all clients
            break;
        }

        std::string message(buffer);
        std::cout << "Received: " << message << std::endl;

        // Detect private message (starts with @username)
        if (message.rfind("@", 0) == 0) {
            // private message starts with e.g. @user1  to show the recipient
            // so the below extracts the recipient and the content
            size_t spacePos = message.find(" ");
            if (spacePos != std::string::npos) {
                std::string recipient = message.substr(1, spacePos - 1);
                std::string content = message.substr(spacePos + 1);

                // find the recipient client and send the private message to it
                std::lock_guard<std::mutex> lock(clients_mutex);
                if (clientUserMap.find(recipient) != clientUserMap.end()) {
                    SOCKET recipientSocket = clientUserMap[recipient];
                    std::string formattedMessage = "@" + recipient + " " + username + ": " + content;
                    send(recipientSocket, formattedMessage.c_str(), formattedMessage.length(), 0);
                }
            }
        }
        else {
            // for public message
            // Broadcast message to all clients
            clients_mutex.lock();
            for (int i = 0; i < clients.size(); i++) {
                // add the prefix to indicate which user sent this message
                string prefix = "user" + to_string(id) + ": ";
                string reply = prefix + message;
                send(clients.at(i), reply.c_str(), reply.length(), 0);
                
            }
            clients_mutex.unlock();
        }
        
    }
}


// the main function of the server that connect with client
int server_loop_multi() {
    // Step 1: Initialize WinSock
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        std::cerr << "WSAStartup failed with error: " << WSAGetLastError() << std::endl;
        return 1;
    }

    // Step 2: Create a socket
    SOCKET server_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (server_socket == INVALID_SOCKET) {
        std::cerr << "Socket creation failed with error: " << WSAGetLastError() << std::endl;
        WSACleanup();
        return 1;
    }

    // Step 3: Bind the socket
    sockaddr_in server_address = {};
    server_address.sin_family = AF_INET;
    server_address.sin_port = htons(65432);  // Server port
    server_address.sin_addr.s_addr = INADDR_ANY; // Accept connections on any IP address

    if (bind(server_socket, (sockaddr*)&server_address, sizeof(server_address)) == SOCKET_ERROR) {
        std::cerr << "Bind failed with error: " << WSAGetLastError() << std::endl;
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }

    // Step 4: Listen for incoming connections
    if (listen(server_socket, SOMAXCONN) == SOCKET_ERROR) {
        std::cerr << "Listen failed with error: " << WSAGetLastError() << std::endl;
        closesocket(server_socket);
        WSACleanup();
        return 1;
    }

    std::cout << "Server is listening on port 65432..." << std::endl;

    int connections = 0;
    while (true) {
        // Step 5: Accept a connection
        sockaddr_in client_address = {};
        int client_address_len = sizeof(client_address);
        SOCKET client_socket = accept(server_socket, (sockaddr*)&client_address, &client_address_len);
        if (client_socket == INVALID_SOCKET) {
            std::cerr << "Accept failed with error: " << WSAGetLastError() << std::endl;
            closesocket(server_socket);
            WSACleanup();
            return 1;
        }

        char client_ip[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_address.sin_addr, client_ip, INET_ADDRSTRLEN);
        std::cout << "Accepted connection from " << client_ip << ":" << ntohs(client_address.sin_port) << std::endl;
        
        // store the client socket
        clients.push_back(client_socket);
        
        std::thread t = std::thread(HandleClient, client_socket, ++connections); // launch a new thread to connect with client
        t.detach();
        //++connections;
    }
    closesocket(server_socket);
    WSACleanup();

    return 0;
}

int main() {
    
    server_loop_multi();
}