#include "ClientManager.hpp"
#include <iostream>
#include <array>
#include <WS2tcpip.h>

void ClientManager::AddClient(const sockaddr_in& addr)
{
	char ipBuffer[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &(addr.sin_addr), ipBuffer, INET_ADDRSTRLEN);
	std::string ip(ipBuffer);
	uint16_t port = ntohs(addr.sin_port);

	if (IsKnownClient(addr)) return;

	Client client;
	client.address = addr;
	client.ipAddress = ip;
	client.udpPort = port;
	client.isConnected = true;
	clients.push_back(client);

	std::cout << "New client connected: " << ip << ":" << port << "\n";
}

bool ClientManager::IsKnownClient(const sockaddr_in& addr) const
{
	std::array<char, INET_ADDRSTRLEN> ipBuffer{};
	inet_ntop(AF_INET, &(addr.sin_addr), ipBuffer.data(), ipBuffer.size());
	std::string incomingIP(ipBuffer.data());
	uint16_t port = ntohs(addr.sin_port);

	for (const auto& client : clients) {
		if (client.ipAddress == incomingIP && client.udpPort == port)
			return true;
	}
	return false;
}

const std::vector<Client>& ClientManager::GetClients() const
{
	return clients;
}
