#pragma once
#include <vector>
#include <string>
#include <winsock2.h>
#include <chrono>
#include <unordered_map>

struct Client {
	sockaddr_in address;
	std::string ipAddress;
	uint16_t udpPort = 0;
	bool isConnected = false;

	//ACK/NACK
	uint32_t nextSequenceNumber; // Sequence number to be used for the next packet
	uint32_t lastReceivedSequenceNumber; // Highest sequence number received
	std::chrono::steady_clock::time_point lastHeardFrom;
	std::unordered_map<uint32_t, std::chrono::steady_clock::time_point> recentNackReq;

};

class ClientManager {
public:
	void AddClient(const sockaddr_in& addr);
	bool IsKnownClient(const sockaddr_in& addr) const;
	const std::vector<Client>& GetClients() const;

private:
	std::vector<Client> clients;
};

