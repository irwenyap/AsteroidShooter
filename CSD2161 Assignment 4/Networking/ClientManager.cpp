#include "ClientManager.hpp"
#include <iostream>
#include <array>
#include <WS2tcpip.h>

void ClientManager::AddClient(const sockaddr_in& addr)
{
	char ipBuffer[INET_ADDRSTRLEN];
	inet_ntop(AF_INET, &(addr.sin_addr), ipBuffer, INET_ADDRSTRLEN);
	std::string ip(ipBuffer);

	if (IsKnownClient(addr)) return;

	uint16_t port = ntohs(addr.sin_port);
	Client client;
	client.lastHeartbeatTime = std::chrono::steady_clock::now(); // Initialize heartbeat time
	client.address = addr;
	client.ipAddress = ip;
	client.udpPort = port;
	client.isConnected = true;
	client.clientID = nextClientID++; // Assign and increment the ID
	clients.push_back(client);

	std::cout << "New client connected: " << ip << ":" << port << " (ID: " << client.clientID << ")\n";
}

bool ClientManager::IsKnownClient(const sockaddr_in& addr) const
{
	/*std::array<char, INET_ADDRSTRLEN> ipBuffer{};
	inet_ntop(AF_INET, &(addr.sin_addr), ipBuffer.data(), ipBuffer.size());
	std::string incomingIP(ipBuffer.data());
	uint16_t port = ntohs(addr.sin_port);

	for (const auto& client : clients) {
		if (client.ipAddress == incomingIP && client.udpPort == port)
			return true;
	}
	return false;*/

	auto it = std::find_if(clients.cbegin(), clients.cend(),
		[&addr](const Client& c) {
			return c.address.sin_addr.s_addr == addr.sin_addr.s_addr &&
				c.address.sin_port == addr.sin_port;
			});
	return it != clients.cend();
}

const std::vector<Client>& ClientManager::GetClients() const
{
	return clients;
}

std::vector<Client>& ClientManager::GetClientsNonConst() 
{
	return clients;
}

std::optional<std::reference_wrapper<Client>> ClientManager::GetClientByAddr(const sockaddr_in& addr) {
	auto it = std::find_if(clients.begin(), clients.end(),
		[&addr](const Client& c) {
			return c.address.sin_addr.s_addr == addr.sin_addr.s_addr &&
				c.address.sin_port == addr.sin_port;
		});
	
	if (it != clients.end()) {
		return std::ref(*it);	
	}
	return std::nullopt;
}

std::optional<std::reference_wrapper<const Client>> ClientManager::GetClientByAddr(const sockaddr_in& addr) const
{
	auto it = std::find_if(clients.begin(), clients.end(),
		[&addr](const Client& c) {
			return c.address.sin_addr.s_addr == addr.sin_addr.s_addr &&
				c.address.sin_port == addr.sin_port;
		});

	if (it != clients.end()) {
		return std::ref(*it);
	}
	return std::nullopt;
}

void ClientManager::RemoveClient(const sockaddr_in& addr) {
	clients.erase(
		std::remove_if(clients.begin(), clients.end(),
			[&addr](const Client& c) {
				return c.address.sin_addr.s_addr == addr.sin_addr.s_addr &&
					c.address.sin_port == addr.sin_port;
			}),
		clients.end()
	);

	//std::cout << "Client removed: " << inet_ntoa(addr.sin_addr) << ":" << ntohs(addr.sin_port) << std::endl;
}
