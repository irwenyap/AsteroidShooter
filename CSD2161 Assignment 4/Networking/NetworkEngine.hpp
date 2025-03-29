#pragma once

#include <string>
#include <vector>
#include "winsock2.h"
#include "SocketManager.hpp"
#include "ClientManager.hpp"
#include <set>
#include <map>

using Tick = uint32_t;
using NetworkID = uint32_t;
using SequenceNumber = uint32_t;

// Logical Channel IDs
enum class ChannelId : uint8_t {
	// Reliable & Ordered: Critical state, actions that must be sequential
	RELIABLE_ORDERED = 0,

	// Unreliable & Sequenced: Order matters (newer replaces older), loss okay
	UNRELIABLE_ORDERED = 1, // PlayerUpdate (pos, rot, state) Client->Host -> Host Broadcast

	// System Channels (Reliable, likely Unordered for simplicity here)
	ACKNACK_RELIABLE = 2, // For sending NACKs (and potentially ACKs)
	COMMIT_TICK_RELIABLE = 3, // For broadcasting the commit tick

	MAX_CHANNELS
};

// Channel Properties 
struct ChannelConfig {
	bool isReliable = false;
	bool isOrdered = false;
};

//  Packet Header (Prepended to EVERY packet) 
#pragma pack(push, 1) //use 1 byte alignment 
struct PacketHeader {
	uint8_t channelId;
	SequenceNumber sequenceNumber; // Sequence number *per channel*
};
#pragma pack(pop) //return to default alignment

// Specific System Packet Payloads 
struct NackPacket { // Send on ACKNACK_RELIABLE
	ChannelId targetChannel;
	SequenceNumber missingSequenceStart;
	uint16_t numMissing; // How many sequences are missing starting from start
};

struct CommitTickPacket { // Send on COMMIT_TICK_RELIABLE
	Tick committedTick;
};

// Data stored for reliable packets sent 
struct SentPacketInfo {
	std::vector<char> data; // Full packet data (header + payload)
	std::chrono::steady_clock::time_point timeSent;
	int transmissions = 1;
	// Add ACK state if implementing ACKs later
};

// --- State for a received channel (Client Side) ---
struct ClientChannelState {
	SequenceNumber expectedSequence = 0;     // For ordered channels
	SequenceNumber lastReceivedSequence = 0; // For unreliable-sequenced
	std::set<SequenceNumber> receivedUnorderedSequences; // For unordered reliable (detect duplicates)

	// Buffer for out-of-order packets on *ordered* channels (Sequence -> Full Packet Data)
	std::map<SequenceNumber, std::vector<char>> bufferedPackets;
	// NACK tracking (Sequence -> Time NACK sent) - Avoid spam
	std::map<SequenceNumber, std::chrono::steady_clock::time_point> pendingNacks;
};

// Host's view of a Client's Network State 
struct ClientNetworkState {
	sockaddr_in address;
	NetworkID clientId = 0; // Assign unique IDs
	bool isConnected = false;
	std::chrono::steady_clock::time_point lastHeardFrom;

	// Sending state (Host -> Client)
	std::map<ChannelId, SequenceNumber> nextSequenceToSend; // Seq# to send next ON THIS CHANNEL
	std::map<std::pair<ChannelId, SequenceNumber>, SentPacketInfo> sentReliablePackets; // Buffer for retransmissions

	// Receiving state (Client -> Host) - Optional but good for reliable client->host comms
	std::map<ChannelId, ClientChannelState> receiveState; // Tracks seq numbers received from this client

	// NACK tracking (for packets client claims are missing)
	std::map<std::pair<ChannelId, SequenceNumber>, std::chrono::steady_clock::time_point> recentNackRequests;
};

// Structure to hold events queued by tick
struct QueuedEvent {
	ChannelId channel;
	// Store event data appropriately. Using vector<char> for now.
	// Ideally, deserialize into specific event structs here.
	std::vector<char> payloadData;
};

class NetworkEngine {



public:
	enum CMDID {
		UNKNOWN = (unsigned char)0x0,
		REQ_CONNECTION = (unsigned char)0x1,
		RSP_CONNECTION = (unsigned char)0x2,
		//TICK_SYNC = (unsigned char)0x3,
		//GAME_DATA = (unsigned char)0x4,
		//GAME_EVENT = (unsigned char)0x5
	};
	static NetworkEngine& GetInstance();

	void Initialize();
	void Update(double);
	bool Host(std::string);
	bool Connect(std::string, std::string);
	void Exit();


	// Sending API
	// Send data on a specific channel. Reliability/ordering based on channel config.
	// Host -> Specific Client
	bool SendToClient(NetworkID clientId, ChannelId channel, const std::vector<char>& payload);
	// Host -> All Clients
	void Broadcast(ChannelId channel, const std::vector<char>& payload, NetworkID excludeClientId = 0);
	// Client -> Host
	bool SendToServer(ChannelId channel, const std::vector<char>& payload);


	// --- Event & State Access ---
	size_t GetNumConnectedClients() const;
	inline std::string GetIPAddress() { return socketManager.GetLocalIP(); }
	inline NetworkID GetMyNetworkID() const { return myNetworkId; }
	inline bool IsHost() const { return isHosting; }
	inline bool IsClient() const { return isClient; }
	inline Tick GetLastCommitTick() const { return hostCommittedTick; }
	std::vector<QueuedEvent> GetEventsForTick(Tick tick); // Called by Scene/App to get processed events

private:
	void ProcessIncomingPackets();
	void HandlePacket(const char* buffer, int bytesRead, const sockaddr_in& senderAddr);
	void HandleClientPacket(ClientNetworkState& clientState, ChannelId channel, SequenceNumber sequence, const char* payload, size_t payloadSize);
	void HandleHostPacket(ChannelId channel, SequenceNumber sequence, const char* payload, size_t payloadSize);
	void HandleNackPacket(const char* payload, size_t payloadSize, ClientNetworkState& requestingClient); // Host handles NACK
	void HandleNackPacketClient(const char* payload, size_t payloadSize); // Client handles NACK (if host sends)
	void HandleCommitTickPacket(const char* payload, size_t payloadSize); // Client handles commit tick

	// Internal send helper - adds header, sequence, buffers reliable
	bool InternalSend(SOCKET sock, const sockaddr_in& destAddr, ChannelId channel, SequenceNumber sequence, const std::vector<char>& payload, bool isReliable);
	void SendNack(const sockaddr_in& destAddr, ChannelId targetChannel, SequenceNumber missingSequence); // Used by client

	void UpdateCommitTick(double dt); // Host decides commit tick
	void CheckRetransmissions(); // Host checks for timed-out reliable packets (optional if pure NACK)
	void CheckTimeouts(double dt); // Check client connections

	ChannelId GetChannelForEventType(EventType type); // Maps game events to channels
	void QueueEventForProcessing(Tick tick, ChannelId channel, const char* payload, size_t payloadSize);


	SocketManager socketManager;
	bool isInitialized = false;
	bool isHosting = false;
	bool isClient = false;
	std::string portNumber;
	NetworkID nextNetworkId = 1; // Start IDs > 0
	NetworkID myNetworkId = 0; // Set when connecting or hosting

	// Host State 
	std::map<NetworkID, ClientNetworkState> connectedClients; // Map NetworkID -> State
	std::map<uint64_t, NetworkID> addrToClientIdMap; // Helper to find client by sockaddr_in hash
	Tick currentNetworkTick = 0; // Host's authoritative tick
	Tick lastCommittedTick = 0; // Last tick committed to all clients
	double timeSinceLastCommitTick = 0.0;
	const double commitTickInterval = 0.1; // Send commit tick every 100ms

	// Client State 
	sockaddr_in hostAddress;
	bool isConnecting = false;
	bool isConnected = false;
	Tick hostCommittedTick = 0; // Latest tick confirmed safe by host
	std::map<ChannelId, ClientChannelState> hostChannelStates; // Tracking packets FROM host

	// Buffer for events received but not yet processed (waiting for commit tick / preceding packets)
	std::map<Tick, std::vector<QueuedEvent>> pendingEvents;

	// Channel configuration map
	std::map<ChannelId, ChannelConfig> channelConfigs;

	// Retransmission buffer (Client -> Host reliable packets)
	std::map<std::pair<ChannelId, SequenceNumber>, SentPacketInfo> sentReliablePacketsToServer;
};

