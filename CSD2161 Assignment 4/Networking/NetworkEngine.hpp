#pragma once

#include <string>
#include <vector>
#include <chrono>
#include <functional>
#include "winsock2.h"
#include "SocketManager.hpp"
#include "ClientManager.hpp"
#include "../Events/Event.hpp" 
#include <unordered_set>
#include <unordered_map>

using Tick = uint32_t;
using NetworkID = uint32_t;
using EventID = uint32_t; 
using ClientID = uint32_t;
using TimePoint = std::chrono::steady_clock::time_point;

class NetworkEngine {
public:

	// Timeouts in milliseconds
	static constexpr long long EVENT_TIMEOUT_MS = 3000; // 3 seconds for an event to get all ACKs
	static constexpr long long CLIENT_TIMEOUT_MS = 10000; // 10 seconds without heartbeat = disconnect
	static constexpr long long HEARTBEAT_INTERVAL_MS = 2000; // Client sends heartbeat every 2 seconds



	enum CMDID {
		UNKNOWN = (unsigned char)0x0,
		REQ_CONNECTION = (unsigned char)0x1,
		RSP_CONNECTION = (unsigned char)0x2,
		TICK_SYNC = (unsigned char)0x3,
		GAME_DATA = (unsigned char)0x4, // State updates
		GAME_EVENT = (unsigned char)0x5, // Client -> Host event
		BROADCAST_EVENT = (unsigned char)0x6, // Host -> Client event broadcast
		ACK_EVENT = (unsigned char)0x7, // Client -> Host event acknowledgement
		COMMIT_EVENT = (unsigned char)0x8,  // Host -> Client command to process event
		HEARTBEAT = (unsigned char)0x9, // Client -> Host keep-alive
		PLAYER_LEFT = (unsigned char)0xA, // Host -> Client notification
		INITIAL_STATE_OBJECT = (unsigned char)0xB // Host -> New Client state sync
	};
	static NetworkEngine& GetInstance();

	void Initialize();
	void Update(double);
	bool Host(std::string);
	bool Connect(std::string, std::string);
	void Exit();

	void SendEventToServer(std::unique_ptr<GameEvent> event); // Client function
	void SendToAllClients(std::vector<char> packet);
	void SendToClient(const Client& client, const std::vector<char>& packet); // Specific client send
	void SendToOtherClients(const sockaddr_in& reqClient, std::vector<char> packet);
	void HandleIncomingConnection(const std::vector<char>& data, const sockaddr_in& clientAddr);
	void HandleClientEvent(const std::vector<char>& data, const sockaddr_in& clientAddr = {}); //tmp hack for server to send to itself
	//void SendTickSync(Tick&);
	void ProcessTickSync(Tick&, Tick&);
	//void SendPacket(std::vector<char>);
	size_t GetNumConnectedClients() const;

	inline std::string GetIPAddress() { return socketManager.GetLocalIP(); }
	inline NetworkID GenerateID() { return nextID++; }
	inline EventID GenerateEventID() { return nextEventID++; }

	bool isHosting = false;
	bool isClient = false;
	ClientManager clientManager;
	SocketManager socketManager;
private:
	NetworkEngine() = default;
	~NetworkEngine() = default;

	//std::string serverIPAddr;
	NetworkID nextID = 1; //reserve 0 for server

	EventID nextEventID = 0;
	TimePoint lastHeartbeatSentTime; // Client tracks when it last sent a heartbeat

	
	void HandleAckEvent(const std::vector<char>&data, const sockaddr_in & clientAddr);
	void HandleHeartbeat(const sockaddr_in& clientAddr); // Host handles heartbeat
	void HandleBroadcastEvent(const std::vector<char>&data); // Client side
	void HandleCommitEvent(const std::vector<char>&data);    // Client side
	//void HandleInitialStateObject(const std::vector<char>& data); // Client handles incoming state
	
	void CheckAckTimeouts(); // Host checks periodically for ACK timeouts

	void CheckTimeoutsAndHeartbeats(); // Host checks periodically
	//void SendInitialState(const Client & newClient); // Host sends current game state

	struct PendingEventInfo {
		std::vector<char> eventData; // Store the original event data (EventType + specific data)
		std::unordered_set<ClientID> acksReceived; // Store the client IDs that have acknowledged the event
		
		TimePoint broadcastTime;
	};
	std::unordered_map<EventID, PendingEventInfo> pendingAcks; // Store pending events that need to be acknowledged by clients

	// Client specific state for lockstep
	std::unordered_map<EventID, std::vector<char>> pendingClientEvents; // Store raw event data (EventType + specific data)

	// client stuff
	//bool isClient = false;
	//SOCKET clientUDPSocket = INVALID_SOCKET;
	//Server serverInfo;

	// host stuff
	//bool isHost = false;
	//SOCKET udpListeningSocket = INVALID_SOCKET;
	//std::vector<Client> clientConnections;

	// Game state access - Needs to be set or passed somehow
	std::function<std::unordered_map<NetworkID, NetworkObject*>& ()> getNetworkedObjectsCallback;
};

