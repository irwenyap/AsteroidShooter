#pragma once
#include <vector>
#include <string>
#include <winsock2.h>

struct Client {
	sockaddr_in address;
	std::string ipAddress;
	uint16_t udpPort = 0;
	bool isConnected = false;
};

class ClientManager {
public:
	void AddClient(const sockaddr_in& addr);
	bool IsKnownClient(const sockaddr_in& addr) const;
	const std::vector<Client>& GetClients() const;

private:
	std::vector<Client> clients;
};

