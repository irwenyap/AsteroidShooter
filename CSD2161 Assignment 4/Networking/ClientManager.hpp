#pragma once
#include <vector>
#include <string>
#include <winsock2.h>
#include <optional>
#include <chrono>

// Forward declare NetworkEngine types
using ClientID = uint32_t;
using TimePoint = std::chrono::steady_clock::time_point;

struct Client {
	sockaddr_in address;
	ClientID clientID;
	std::string ipAddress;
	uint16_t udpPort = 0;
	bool isConnected = false;
	TimePoint lastHeartbeatTime; // Track when the host last heard from this client

	// Basic comparison for searching, might need adjustment based on sockaddr_in usage
	bool operator==(const sockaddr_in& other) const {
		return address.sin_addr.s_addr == other.sin_addr.s_addr &&
			address.sin_port == other.sin_port;
	}
};



class ClientManager {
public:
	void AddClient(const sockaddr_in& addr);
	bool IsKnownClient(const sockaddr_in& addr) const;
	const std::vector<Client>& GetClients() const;
	std::vector<Client>& GetClientsNonConst();

	std::optional<std::reference_wrapper<Client>> GetClientByAddr(const sockaddr_in& addr);
	std::optional<std::reference_wrapper<const Client>> GetClientByAddr(const sockaddr_in & addr) const;
	void RemoveClient(const sockaddr_in& addr);

private:
	std::vector<Client> clients;
	ClientID nextClientID = 1; // Start client IDs from 1
};

