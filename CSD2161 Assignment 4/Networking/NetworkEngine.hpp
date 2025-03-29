#pragma once

#include <string>
#include <vector>
#include "winsock2.h"
#include "SocketManager.hpp"
#include "ClientManager.hpp"

using Tick = uint32_t;
using NetworkID = uint32_t;
//
//struct NetworkEvent {
//	Tick tick;
//};

class NetworkEngine {



public:
	enum CMDID {
		UNKNOWN = (unsigned char)0x0,
		REQ_CONNECTION = (unsigned char)0x1,
		RSP_CONNECTION = (unsigned char)0x2,
		TICK_SYNC = (unsigned char)0x3,
		GAME_DATA = (unsigned char)0x4,
		GAME_EVENT = (unsigned char)0x5
	};
	static NetworkEngine& GetInstance();

	void Initialize();
	void Update(double);
	bool Host(std::string);
	bool Connect(std::string, std::string);
	void Exit();

	void SendToAllClients(std::vector<char> packet);
	void SendToOtherClients(const sockaddr_in& reqClient, std::vector<char> packet);
	void HandleIncomingConnection(const std::vector<char>& data, const sockaddr_in& clientAddr);
	//void SendTickSync(Tick&);
	void ProcessTickSync(Tick&, Tick&);
	//void SendPacket(std::vector<char>);
	size_t GetNumConnectedClients() const;

	inline std::string GetIPAddress() { return socketManager.GetLocalIP(); }
	inline NetworkID GenerateID() { return nextID++; }

	bool isHosting = false;
	bool isClient = false;
	ClientManager clientManager;
	SocketManager socketManager;
private:
	NetworkEngine() = default;
	~NetworkEngine() = default;

	//std::string serverIPAddr;
	NetworkID nextID = 0;

	// client stuff
	//bool isClient = false;
	//SOCKET clientUDPSocket = INVALID_SOCKET;
	//Server serverInfo;

	// host stuff
	//bool isHost = false;
	//SOCKET udpListeningSocket = INVALID_SOCKET;
	//std::vector<Client> clientConnections;
};

