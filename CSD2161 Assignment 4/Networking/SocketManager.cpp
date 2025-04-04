#include "SocketManager.hpp"
#include <iostream>
#include "ws2tcpip.h"

#pragma comment(lib, "ws2_32.lib")

#define MAX_PACKET_SIZE 1472

bool SocketManager::Host(const std::string& port) {
	char hostname[256];
	gethostname(hostname, sizeof(hostname));

    addrinfo hints{}, *info = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;
    hints.ai_flags = AI_PASSIVE;

    if (getaddrinfo(hostname, port.c_str(), &hints, &info) != 0 || !info) {
        std::cerr << "getaddrinfo failed for host.\n";
        return false;
    }

    udpListeningSocket = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
    if (udpListeningSocket == INVALID_SOCKET) {
        std::cerr << "Failed to create host socket.\n";
        freeaddrinfo(info);
        return false;
    }

    if (bind(udpListeningSocket, info->ai_addr, static_cast<int>(info->ai_addrlen)) != 0) {
        std::cerr << "Failed to bind host socket.\n";
        freeaddrinfo(info);
        closesocket(udpListeningSocket);
        udpListeningSocket = INVALID_SOCKET;
        return false;
    }

    inet_ntop(AF_INET, &(((sockaddr_in*)info->ai_addr)->sin_addr), hostname, sizeof(hostname));
    localIP = hostname;

    SetNonBlocking(udpListeningSocket);
    freeaddrinfo(info);
    return true;
}

bool SocketManager::Connect(const std::string& ip, const std::string& port)
{
    addrinfo hints{}, * info = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_DGRAM;
    hints.ai_protocol = IPPROTO_UDP;

    if (getaddrinfo(ip.c_str(), port.c_str(), &hints, &info) != 0 || !info) {
        std::cerr << "getaddrinfo failed for client.\n";
        return false;
    }

    clientSocket = socket(info->ai_family, info->ai_socktype, info->ai_protocol);
    if (clientSocket == INVALID_SOCKET) {
        std::cerr << "Failed to create client socket.\n";
        freeaddrinfo(info);
        return false;
    }

    serverInfo.ipAddress = ip;
    serverInfo.port = std::stoi(port);
    memcpy(&serverInfo.address, info->ai_addr, sizeof(serverInfo.address));
    freeaddrinfo(info);

	return true;
}

bool SocketManager::ConnectWithHandshake(const std::string& ip, const std::string& port, uint8_t sendCommand, uint8_t expectedResponse,
    const std::string& playerName)
{
    if (!Connect(ip, port)) return false;

    const int maxRetries = 10;
    int retryCount = 0;
    bool connectionEstablished = false;
    uint8_t cmd = sendCommand;
    fd_set readfds;
    timeval timeout = { 1, 0 }; // 1 second timeout

    while (!connectionEstablished && retryCount < maxRetries) {
        std::vector<char> packet;
        packet.push_back(static_cast<char>(cmd));

        uint8_t nameLen = static_cast<uint8_t>(std::min<size_t>(playerName.size(), 255));
        packet.push_back(static_cast<char>(nameLen));

        packet.insert(packet.end(), playerName.begin(), playerName.begin() + nameLen);


        bool success = SendToHost(packet);
        if (!success) {
            std::cerr << "sendto() failed. Error: " << WSAGetLastError() << "\n";
            break;
        }

        FD_ZERO(&readfds);
        FD_SET(clientSocket, &readfds);
        int selResult = select(0, &readfds, NULL, NULL, &timeout);
        if (selResult > 0 && FD_ISSET(clientSocket, &readfds)) {
            char response;
            sockaddr_in serverAddr;
            int addrLen = sizeof(serverAddr);
            int recvResult = recvfrom(clientSocket, &response, sizeof(response), 0,
                reinterpret_cast<sockaddr*>(&serverAddr), &addrLen);
            if (recvResult > 0 && static_cast<uint8_t>(response) == expectedResponse) {
                connectionEstablished = true;
                //serverInfo.ipAddress = ip;
                //serverInfo.port = std::stoi(port);
                //memcpy(&serverInfo.address, &serverAddr, sizeof(serverAddr));
                std::cout << "Handshake response received from server.\n";
                break;
            }
        }
        retryCount++;
        std::cout << "Retry " << retryCount << "...\n";
    }

    if (!connectionEstablished) {
        std::cerr << "Handshake failed after retries.\n";
        closesocket(clientSocket);
        clientSocket = INVALID_SOCKET;
        return false;
    }

    SetNonBlocking(clientSocket);
    return true;
}

void SocketManager::Cleanup()
{
    if (udpListeningSocket != INVALID_SOCKET) {
        closesocket(udpListeningSocket);
        udpListeningSocket = INVALID_SOCKET;
    }
    if (clientSocket != INVALID_SOCKET) {
        closesocket(clientSocket);
        clientSocket = INVALID_SOCKET;
    }
}

bool SocketManager::SendToClient(const sockaddr_in& clientAddr, const std::vector<char>& data)
{
    return sendto(udpListeningSocket, data.data(), static_cast<int>(data.size()), 0,
        reinterpret_cast<const sockaddr*>(&clientAddr), sizeof(clientAddr)) != SOCKET_ERROR;
}

bool SocketManager::SendToClient(const sockaddr_in& clientAddr, const char& data)
{
    return sendto(udpListeningSocket, &data, sizeof(data), 0,
        reinterpret_cast<const sockaddr*>(&clientAddr), sizeof(clientAddr)) != SOCKET_ERROR;
}

bool SocketManager::SendToHost(const std::vector<char>& data)
{
    return sendto(clientSocket, data.data(), static_cast<int>(data.size()), 0,
        reinterpret_cast<sockaddr*>(&serverInfo.address), sizeof(serverInfo.address)) != SOCKET_ERROR;
}

bool SocketManager::SendToHost(const char& data)
{
    return sendto(clientSocket, &data, sizeof(data), 0,
        reinterpret_cast<sockaddr*>(&serverInfo.address), sizeof(serverInfo.address)) != SOCKET_ERROR;
}

bool SocketManager::ReceiveFromClient(std::vector<char>& outData, sockaddr_in& outAddr)
{
    char buffer[MAX_PACKET_SIZE];
    int addrLen = sizeof(outAddr);
    int result = recvfrom(udpListeningSocket, buffer, sizeof(buffer), 0,
        reinterpret_cast<sockaddr*>(&outAddr), &addrLen);
    if (result > 0) {
        outData.assign(buffer, buffer + result);
        return true;
    }
    return false;
}

bool SocketManager::ReceiveFromHost(std::vector<char>& outData)
{
    char buffer[MAX_PACKET_SIZE];
    int addrLen = sizeof(serverInfo.address);
    int result = recvfrom(clientSocket, buffer, sizeof(buffer), 0,
        reinterpret_cast<sockaddr*>(&serverInfo.address), &addrLen);
    if (result > 0) {
        outData.assign(buffer, buffer + result);
        return true;
    }
    return false;
}

void SocketManager::SetNonBlocking(SOCKET sock)
{
    u_long mode = 1;
    ioctlsocket(sock, FIONBIO, &mode);
}
