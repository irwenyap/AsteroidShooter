#pragma once

#include <string>
#include <vector>
#include "WinSock2.h"

class SocketManager {
	struct Server {
		sockaddr_in address;
		std::string ipAddress;
		uint16_t port = 0;
	};
public:
	bool Host(const std::string& port);
	bool Connect(const std::string& ip, const std::string& port);
	bool ConnectWithHandshake(const std::string& ip, const std::string& port, 
		uint8_t sendCommand, uint8_t expectedResponse);
	void Cleanup();

	bool SendToClient(const sockaddr_in& clientAddr, const std::vector<char>& data);
	bool SendToClient(const sockaddr_in& clientAddr, const char& data);
	bool SendToHost(const std::vector<char>& data);
	bool SendToHost(const char& data);

	bool ReceiveFromClient(std::vector<char>& outData, sockaddr_in& outAddr);
	bool ReceiveFromHost(std::vector<char>& outData);
	int Receive(SOCKET sock, char* buffer, int len, sockaddr_in& sender);

	inline std::string GetLocalIP() const {
		return localIP;
	}

	const sockaddr_in& GetServerAddress() const { return serverInfo.address; }
	SOCKET GetClientSocket() const { return clientSocket; }
	SOCKET GetHostSocket() const { return udpListeningSocket; }
	void SetNonBlocking(SOCKET sock);

private:
	SOCKET udpListeningSocket = INVALID_SOCKET;
	SOCKET clientSocket = INVALID_SOCKET;

	Server serverInfo;
	std::string localIP;
};

