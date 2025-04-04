#include "NetworkEngine.hpp"

#include <iostream>

#include "ws2tcpip.h"		// getaddrinfo()

#include "../Events/EventQueue.hpp"
#include "../AsteroidScene.hpp" // HACK: Include scene for now for state access.
#include <thread>
#include <algorithm>

// Tell the Visual Studio linker to include the following library in linking.
// Alternatively, we could add this file to the linker command-line parameters,
// but including it in the source code simplifies the configuration.
#pragma comment(lib, "ws2_32.lib")

#ifndef WINSOCK_VERSION
#define WINSOCK_VERSION     2
#endif
#define WINSOCK_SUBVERSION  2
#define MAX_STR_LEN         1000

//extern Tick simulationTick;
//extern Tick localTick;

// HACK: Temporary global scene pointer for state sync.
extern AsteroidScene * g_AsteroidScene;

NetworkEngine& NetworkEngine::GetInstance() {
	static NetworkEngine ne;
	return ne;
}

void NetworkEngine::Initialize() {
	WSADATA wsaData{};
	SecureZeroMemory(&wsaData, sizeof(wsaData));

	int errorCode = WSAStartup(MAKEWORD(WINSOCK_VERSION, WINSOCK_SUBVERSION), &wsaData);
	if (NO_ERROR != errorCode) {
		std::cerr << "WSAStartup() failed.\n";
		return;
	}
}

void NetworkEngine::Update(double) {
	std::vector<char> data;
	sockaddr_in sender{};

	if (isHosting) {

		if (simulationTick % 120 == 0) { // Every 120 ticks (2 second) send sync.
			SendTickSync();
		}

		// Resend packets that have timed out
		CheckAckTimeouts();

		// Check for client timeouts and timed-out events before processing new packets
		CheckTimeoutsAndHeartbeats();

		while (socketManager.ReceiveFromClient(data, sender)) {
			
			if (data.empty()) continue;

			switch (static_cast<CMDID>(data[0]))
			{
			case REQ_CONNECTION:
				HandleIncomingConnection(data, sender);
				break;
			case GAME_DATA: // Player position updates (state sync)
				// Optional: Could add client ID verification here
				EventQueue::GetInstance().Push(std::make_unique<PlayerUpdate>(data.data(), data.size()));
				SendToOtherClients(sender, data); // Still broadcast state updates immediately
				break;
			case GAME_EVENT: // Client submitting an action event for lockstep
				HandleClientEvent(data);
				break;
			case ACK_EVENT: // Client acknowledging receipt of a broadcast event
				HandleAckEvent(data, sender);
				break;
			case HEARTBEAT: // Client sending keep-alive
				HandleHeartbeat(sender);
				break;
			default:
				// Optional: Log unknown packet type
				break;
			}
		}
	} else if (isClient) {

		// Client-side heartbeat sending
		auto now = std::chrono::steady_clock::now();
		if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastHeartbeatSentTime).count() >= HEARTBEAT_INTERVAL_MS) {
			char heartbeatCmd = CMDID::HEARTBEAT;
			socketManager.SendToHost(heartbeatCmd);
			lastHeartbeatSentTime = now;
		}

		//auto now = std::chrono::steady_clock::now();
		auto timeSinceLastResponse = std::chrono::duration_cast<std::chrono::seconds>(
			now - lastServerResponseTime
		).count();

		// Detect disconnection
		//if (timeSinceLastResponse > 5 && !isAttemptingReconnect) { // 5 seconds timeout
		//	std::cout << "Connection lost. Attempting to reconnect..." << std::endl;
		//	isAttemptingReconnect = true;
		//	AttemptReconnect();
		//}

		while (socketManager.ReceiveFromHost(data)) {
			if (data.empty()) continue;

			lastServerResponseTime = std::chrono::steady_clock::now();

			switch (static_cast<CMDID>(data[0])) {
			case TICK_SYNC: {
				Tick receivedTick;
				std::memcpy(&receivedTick, &data[1], sizeof(receivedTick));
				localTick = ntohl(receivedTick);
				break;
			}
			case GAME_DATA: {
				EventQueue::GetInstance().Push(std::make_unique<PlayerUpdate>(data.data(), data.size()));
				break;
			}
			case BROADCAST_EVENT: // Host broadcasting an event for lockstep
				HandleBroadcastEvent(data);
				break;
			case COMMIT_EVENT: // Host commanding the client to process a specific event
				HandleCommitEvent(data);
				break;
			case INITIAL_STATE_OBJECT: // Host sending initial state for an object
				// HandleInitialStateObject(data);
				break;
			case FULL_STATE_SNAPSHOT:
				HandleFullStateSnapshot(data);
				break;
			default:
				// Optional: Log unknown packet type
				break;
			}
		}
	}
}


bool NetworkEngine::Host(std::string portNumber) {
	isHosting = socketManager.Host(portNumber);

	if (isHosting) {
		isClient = false; 
		std::cout << "Hosting on IP: " << GetIPAddress() << " Port: " << portNumber << std::endl;
	}
	return isHosting;
}

bool NetworkEngine::Connect(std::string host, std::string portNumber, const std::string& playerName) {
	isClient = socketManager.ConnectWithHandshake(host, portNumber,
		CMDID::REQ_CONNECTION, CMDID::RSP_CONNECTION, playerName);

	if (isClient) {
		isHosting = false;
		std::cout << "Connected to host: " << host << " Port: " << portNumber << std::endl;
	}

	return isClient;
}

void NetworkEngine::Exit() {	
	socketManager.Cleanup();
	WSACleanup();
}

void NetworkEngine::AttemptReconnect() {
	std::thread([this]() {
		while (isAttemptingReconnect) {
			if (socketManager.ConnectWithHandshake(socketManager.serverInfo.ipAddress, std::to_string(socketManager.serverInfo.port),
				CMDID::REQ_RECONNECT, CMDID::RSP_RECONNECT, "hello")) {
				std::cout << "Reconnected successfully!" << std::endl;
				isAttemptingReconnect = false;

				// Send reconnect confirmation
				//std::vector<char> packet;
				//packet.push_back(CMDID::RECONNECT_INFO);
				//NetworkID netID = htonl(clientNetworkID);
				//packet.insert(packet.end(),
				//	reinterpret_cast<char*>(&netID),
				//	reinterpret_cast<char*>(&netID) + sizeof(netID));
				//socketManager.SendToHost(packet);
				return;
			}

			std::this_thread::sleep_for(std::chrono::seconds(3)); // Retry every 3 seconds
		}
		}).detach();
}

// Client function to send an event to the server for lockstep processing
void NetworkEngine::SendEventToServer(std::unique_ptr<GameEvent> eventt) {
	if (!isClient) return;

	std::vector<char> packet;
	packet.push_back(CMDID::GAME_EVENT); // Mark as client-submitted event
	packet.push_back(static_cast<char>(eventt->type)); // Add the event type

	// Serialize the specific event data
	switch (eventt->type) {
	case EventType::FireBullet: {
		auto fireEvent = static_cast<FireBulletEvent*>(eventt.get());
		std::vector<char> eventData = fireEvent->Serialize();
		packet.insert(packet.end(), eventData.begin(), eventData.end());
		break;
	}
	case EventType::Collision: {
		auto collisionEvent = static_cast<CollisionEvent*>(eventt.get());
		std::vector<char> eventData = collisionEvent->Serialize();
		packet.insert(packet.end(), eventData.begin(), eventData.end());
		break;
	} 
	}// end switch


	if (packet.size() > 2) { // Ensure we actually added event data
		socketManager.SendToHost(packet);
	}
	else {
		std::cerr << "Warning: Tried to send unknown or empty event type: " << static_cast<int>(eventt->type) << std::endl;
	}
}

void NetworkEngine::ServerBroadcastEvent(std::unique_ptr<GameEvent> event) 
{
	if (!isHosting) return;

	std::vector<char> data;
	switch (event->type) {
	case EventType::FireBullet: {
		auto fireEvent = static_cast<FireBulletEvent*>(event.get());
		data.push_back(static_cast<char>(EventType::FireBullet));
		std::vector<char> eventData = fireEvent->Serialize();
		data.insert(data.end(), eventData.begin(), eventData.end());
		break;
	}
	} // end switch

	EventID currentEventID = nextEventID++;
	EventType eventType = event->type;

	PendingEventInfo info;
	info.eventData = std::move(data);
	info.broadcastTime = std::chrono::steady_clock::now();
	pendingAcks[currentEventID] = std::move(info);
	
	// Prepare broadcast packet
	std::vector<char> broadcastPacket;
	broadcastPacket.push_back(CMDID::BROADCAST_EVENT); // Mark as broadcast event
	EventID netEventID = htonl(currentEventID);
	broadcastPacket.insert(broadcastPacket.end(), reinterpret_cast<char*>(&netEventID), reinterpret_cast<char*>(&netEventID) + sizeof(netEventID));
	// Append the original event data (EventType + SpecificData)
	broadcastPacket.insert(broadcastPacket.end(), 
		pendingAcks[currentEventID].eventData.begin(), pendingAcks[currentEventID].eventData.end());

	std::cout << "[Host] Broadcasting Event ID: " << currentEventID << std::endl;
	SendToAllClients(broadcastPacket); // Broadcast to everyone
}

void NetworkEngine::SendToAllClients(std::vector<char> packet)
{
	for (auto& client : clientManager.GetClients()) {
		socketManager.SendToClient(client.address, packet);
	}
}

void NetworkEngine::SendToClient(const Client & client, const std::vector<char>&packet)
{
	if (isHosting && client.isConnected) {
		socketManager.SendToClient(client.address, packet);
	}
}

void NetworkEngine::SendtoClientSameEvent(const Client& client, EventID eid, const std::vector<char>& data) {
	// Store event data for ACK tracking (skip CMDID)
	PendingEventInfo info;
	info.eventData.assign(data.begin() + 1, data.end()); // Store EventType + SpecificData
	info.broadcastTime = std::chrono::steady_clock::now(); // Record broadcast time
	pendingAcks[eid] = std::move(info);

	// Prepare broadcast packet
	std::vector<char> broadcastPacket;
	broadcastPacket.push_back(CMDID::BROADCAST_EVENT);
	EventID netEventID = htonl(eid);
	broadcastPacket.insert(broadcastPacket.end(), reinterpret_cast<char*>(&netEventID), reinterpret_cast<char*>(&netEventID) + sizeof(netEventID));
	// Append the original event data (EventType + SpecificData)
	broadcastPacket.insert(broadcastPacket.end(), data.begin() + 1, data.end());

	std::cout << "[Host] Broadcasting Event ID: " << eid << std::endl;
	SendToClient(client, broadcastPacket); // Broadcast to everyone
}

void NetworkEngine::SendToOtherClients(const sockaddr_in& reqClient, std::vector<char> packet)
{
	for (auto& client : clientManager.GetClients()) {
		if (client.address.sin_addr.s_addr == reqClient.sin_addr.s_addr &&
			client.address.sin_port == reqClient.sin_port) continue;

		socketManager.SendToClient(client.address, packet);	
	}
}

// Host side handling
void NetworkEngine::HandleIncomingConnection(const std::vector<char>& data, const sockaddr_in& clientAddr)
{
	if (data.empty() || (data[0] != REQ_CONNECTION && data[0] != REQ_RECONNECT)) return;

	if (data[0] == CMDID::REQ_RECONNECT) {
		// Find existing client
		auto clientOpt = clientManager.GetClientByAddr(clientAddr);
		if (clientOpt.has_value()) {
			auto& client = clientOpt.value().get();
			//client.address = clientAddr; // Update address
			client.isConnected = true;
			socketManager.SendToClient(clientAddr, static_cast<char>(CMDID::RSP_RECONNECT));

			// Resend game state
			//SendFullStateUpdate(client);
			SendFullStateSnapshot(clientAddr);
		}
	} else if (!clientManager.IsKnownClient(clientAddr)) {
		uint8_t nameLen = 0;
		if (data.size() > 1) {
			nameLen = static_cast<uint8_t>(data[1]);
		}
		// ensure we have enough bytes:
		if (data.size() < 2 + nameLen) {
			std::cerr << "[Host] Invalid REQ_CONNECTION: Not enough data for name.\n";
			return;
		}
		std::string playerName(data.begin() + 2, data.begin() + 2 + nameLen);

		clientManager.AddClient(clientAddr);
		//EventQueue::GetInstance().Push(std::make_unique<ClientJoinedEvent>());

		auto newClientOpt = clientManager.GetClientByAddr(clientAddr);
		if (newClientOpt) {
			socketManager.SendToClient(clientAddr, static_cast<char>(RSP_CONNECTION)); // Send ACK first
			auto& clientRef = newClientOpt.value().get();
			playerNames[clientRef.clientID] = playerName;
			//SendInitialState(newClientOpt.value().get()); // Then send current state

		}
	}
}

void NetworkEngine::SendTickSync() {
	std::vector<char> packet;
	packet.reserve(5);
	packet.push_back(CMDID::TICK_SYNC);
	NetworkUtils::WriteToPacket(packet, simulationTick, NetworkUtils::DATA_TYPE::DT_LONG);
	SendToAllClients(packet);
}

void NetworkEngine::HandleClientEvent(const std::vector<char>& data) {
	if (data.size() < 2) return; // Need at least CMDID and EventType

	// Optional: Verify client is known
	// auto clientOpt = clientManager.GetClientByAddr(clientAddr);
	// if (!clientOpt) return;

	EventID currentEventID = nextEventID++;
	EventType eventType = static_cast<EventType>(data[1]);

	std::cout << "[Host] Received GAME_EVENT (Type: " << static_cast<int>(eventType) << "), Assigning ID: " << currentEventID << std::endl;

	// Store event data for ACK tracking (skip CMDID)
	PendingEventInfo info;	
	info.eventData.assign(data.begin() + 1, data.end()); // Store EventType + SpecificData
	info.broadcastTime = std::chrono::steady_clock::now(); // Record broadcast time
	pendingAcks[currentEventID] = std::move(info);

	// Prepare broadcast packet
	std::vector<char> broadcastPacket;
	broadcastPacket.push_back(CMDID::BROADCAST_EVENT);
	EventID netEventID = htonl(currentEventID);
	broadcastPacket.insert(broadcastPacket.end(), reinterpret_cast<char*>(&netEventID), reinterpret_cast<char*>(&netEventID) + sizeof(netEventID));
	// Append the original event data (EventType + SpecificData)
	broadcastPacket.insert(broadcastPacket.end(), data.begin() + 1, data.end());

	std::cout << "[Host] Broadcasting Event ID: " << currentEventID << std::endl;
	SendToAllClients(broadcastPacket); // Broadcast to everyone
}

void NetworkEngine::HandleHeartbeat(const sockaddr_in& clientAddr) {
	auto clientOpt = clientManager.GetClientByAddr(clientAddr);
	if (clientOpt) {
		//std::cout << "[Host] Received Heartbeat from Client ID: " << clientOpt.value().get().clientID << std::endl;
		clientOpt.value().get().lastHeartbeatTime = std::chrono::steady_clock::now();
		
		// Keep the client marked as connected
		if (!clientOpt.value().get().isConnected) {
			std::cout << "[Host] Reconnected Client ID: " << clientOpt.value().get().clientID << std::endl;
			clientOpt.value().get().isConnected = true;
			
		}
		
	}
	else {
		std::cerr << "[Host] Received Heartbeat from unknown client." << std::endl;
		         // Optionally send a disconnect or ignore
			
	}
}

void NetworkEngine::HandleAckEvent(const std::vector<char>& data, const sockaddr_in& clientAddr) {
	if (data.size() < 1 + sizeof(EventID)) return; // Need CMDID + EventID

	EventID eventID;
	std::memcpy(&eventID, &data[1], sizeof(EventID));
	eventID = ntohl(eventID);

	auto clientOpt = clientManager.GetClientByAddr(clientAddr);
	if (!clientOpt) {
		std::cerr << "[Host] Received ACK for event " << eventID << " from unknown client." << std::endl;
		return;
	}
	ClientID senderClientID = clientOpt.value().get().clientID;


	auto it = pendingAcks.find(eventID);
	if (it == pendingAcks.end()) {
		std::cerr << "[Host] Received ACK for unknown or already committed Event ID: " << eventID << " from Client " << senderClientID << std::endl;
		return; // Ignore ACK for unknown/committed event
	}

	// Record the ACK
	auto& pendingInfo = it->second;
	auto insertResult = pendingInfo.acksReceived.insert(senderClientID);

	if (insertResult.second) { // Check if insert actually happened (avoid double counting)
		std::cout 
			<< "[Host] Received ACK for Event ID: " << eventID << " from Client " << senderClientID
			<< " (" << pendingInfo.acksReceived.size() << "/" << clientManager.GetClients().size() << ")" 
			<< std::endl;
	}

	// Check if all connected clients have ACKed
	if (pendingInfo.acksReceived.size() >= GetNumConnectedClients()) {
		std::cout << "[Host] All ACKs received for Event ID: " << eventID << ". Committing." << std::endl;

		// Prepare commit packet
		std::vector<char> commitPacket;
		commitPacket.push_back(CMDID::COMMIT_EVENT);
		EventID netEventID = htonl(eventID);
		NetworkID newNetworkID = htonl(nextID++);
		commitPacket.insert(commitPacket.end(), reinterpret_cast<char*>(&netEventID), reinterpret_cast<char*>(&netEventID) + sizeof(netEventID));
		commitPacket.insert(commitPacket.end(), reinterpret_cast<char*>(&newNetworkID), reinterpret_cast<char*>(&newNetworkID) + sizeof(newNetworkID));
		// Send commit command to all clients
		SendToAllClients(commitPacket);


		// Process the event locally
		EventType eventType = static_cast<EventType>(pendingInfo.eventData[0]);
		std::vector<char>& eventData = pendingInfo.eventData;
		size_t offset = 1; // Skip the EventType byte
		switch (eventType) {
		case EventType::FireBullet: {
			if (eventData.size() < offset + (sizeof(float) * 3) + sizeof(float) + sizeof(uint32_t)) { // Basic size check for vec3 + float + uint32
				std::cerr << "[Host] Insufficient data for FireBulletEvent ID: " << eventID << std::endl;
				break;
			}
			glm::vec3 pos;
			float rot;
			uint32_t ownerId;
			NetworkUtils::ReadVec3(eventData.data(), offset, pos); offset += 12; // 3 * 4 bytes for vec3
			uint32_t tempRot;
			NetworkUtils::ReadFromPacket(eventData.data(), offset, tempRot, NetworkUtils::DATA_TYPE::DT_LONG); offset += 4;
			rot = NetworkUtils::NetworkToFloat(tempRot);
			uint32_t tempOwner;
			NetworkUtils::ReadFromPacket(eventData.data(), offset, tempOwner, NetworkUtils::DATA_TYPE::DT_LONG); offset += 4;
			ownerId = tempOwner;

			auto it = std::make_unique<FireBulletEvent>(pos, rot, ownerId);
			it->id = ntohl(newNetworkID);

			EventQueue::GetInstance().Push(std::move(it));
			break;
		}
		case EventType::Collision: {
			NetworkID a, b;
			std::memcpy(&a, &eventData[offset], sizeof(NetworkID)); offset += sizeof(NetworkID);
			std::memcpy(&b, &eventData[offset], sizeof(NetworkID)); offset += sizeof(NetworkID);
			a = ntohl(a);
			b = ntohl(b);
			auto it = std::make_unique<CollisionEvent>(a, b);
			it->id = ntohl(newNetworkID);
			EventQueue::GetInstance().Push(std::move(it));
			break;
		}
		case EventType::StartGame: {
			
			
			
			break;
		}
		default: {
			std::cerr << "[Host] Cannot process unknown committed event type: " << static_cast<int>(eventType) << std::endl;
			break;
		}
			
		} // end switch


		// Remove event from pending list
		pendingAcks.erase(it);
	}
}

//Client-Side Handling
void NetworkEngine::HandleBroadcastEvent(const std::vector<char>& data) {
	if (data.size() < 1 + sizeof(EventID) + 1) return; // CMDID + EventID + EventType

	EventID eventID;
	std::memcpy(&eventID, &data[1], sizeof(EventID));
	eventID = ntohl(eventID);

	EventType eventType = static_cast<EventType>(data[1 + sizeof(EventID)]);

	if (pendingClientEvents.find(eventID) != pendingClientEvents.end()) {
		std::cerr << "[Client] Received duplicate BROADCAST_EVENT for ID: " << eventID << std::endl;
	}
	else {
		std::cout << "[Client] Received BROADCAST_EVENT (Type: " << static_cast<int>(eventType) << ") ID: " << eventID << std::endl;
		// Store the event data (excluding CMDID and EventID) for later processing
		// Start copying after the EventID
		pendingClientEvents[eventID].assign(data.begin() + 1 + sizeof(EventID), data.end());
	}

	// Send ACK back to host
	std::vector<char> ackPacket;
	ackPacket.push_back(CMDID::ACK_EVENT);
	EventID netEventID = htonl(eventID);
	ackPacket.insert(ackPacket.end(), reinterpret_cast<char*>(&netEventID), reinterpret_cast<char*>(&netEventID) + sizeof(netEventID));
	socketManager.SendToHost(ackPacket);

	std::cout << "[Client] Sent ACK for Event ID: " << eventID << std::endl;

}

void NetworkEngine::HandleCommitEvent(const std::vector<char>& data) {
	if (data.size() < 1 + sizeof(EventID)) return; // CMDID + EventID

	EventID eventID;
	std::memcpy(&eventID, &data[1], sizeof(EventID));
	eventID = ntohl(eventID);

	NetworkID networkID;
	std::memcpy(&networkID, &data[1 + sizeof(EventID)], sizeof(NetworkID));
	networkID = ntohl(networkID);

	std::cout << "[Client] Received COMMIT_EVENT for ID: " << eventID << std::endl;


	auto it = pendingClientEvents.find(eventID);
	if (it == pendingClientEvents.end()) {
		std::cerr << "[Client] Received COMMIT_EVENT for unknown Event ID: " << eventID << std::endl;
		return; // Cannot process unknown event
	}

	// Retrieve the stored event data
	const std::vector<char>& eventData = it->second;
	if (eventData.empty()) {
		std::cerr << "[Client] Stored event data for ID: " << eventID << " is empty." << std::endl;
		pendingClientEvents.erase(it);
		return;
	}

	EventType eventType = static_cast<EventType>(eventData[0]);
	std::cout << "[Client] Processing Event ID: " << eventID << " (Type: " << static_cast<int>(eventType) << ")" << std::endl;

	// Reconstruct and push the event to the local queue
	// Need to skip the EventType byte in eventData when passing to specific event constructors/deserializers
	size_t offset = 1; // Start reading data *after* the EventType byte

	switch (eventType) {
	case EventType::FireBullet: {
		if (eventData.size() < offset + (sizeof(float) * 3) + sizeof(float) + sizeof(uint32_t)) { // Basic size check for vec3 + float + uint32
			std::cerr << "[Client] Insufficient data for FireBulletEvent ID: " << eventID << std::endl;
			break;
		}
		glm::vec3 pos;
		float rot;
		uint32_t ownerId;
		NetworkUtils::ReadVec3(eventData.data(), offset, pos); offset += 12; // 3 * 4 bytes for vec3
		uint32_t tempRot;
		NetworkUtils::ReadFromPacket(eventData.data(), offset, tempRot, NetworkUtils::DATA_TYPE::DT_LONG); offset += 4;
		rot = NetworkUtils::NetworkToFloat(tempRot);
		uint32_t tempOwner;
		NetworkUtils::ReadFromPacket(eventData.data(), offset, tempOwner, NetworkUtils::DATA_TYPE::DT_LONG); offset += 4;
		ownerId = tempOwner;

		auto it = std::make_unique<FireBulletEvent>(pos, rot, ownerId);
		it->id = networkID;

		EventQueue::GetInstance().Push(std::move(it));
		break;
	}
	case EventType::StartGame: {
		int offset = 2;
		for (uint8_t i = 0; i < eventData[1]; ++i) {
			uint8_t eventType = eventData[offset++];
			NetworkID networkID;
			std::memcpy(&networkID, &eventData[offset], sizeof(networkID));
			networkID = ntohl(networkID);
			offset += sizeof(networkID);

			switch (eventType) {
			case static_cast<uint8_t>(EventType::SpawnPlayer):
				EventQueue::GetInstance().Push(std::make_unique<SpawnPlayerEvent>(networkID));
				break;
			case static_cast<uint8_t>(EventType::PlayerJoined):
				EventQueue::GetInstance().Push(std::make_unique<PlayerJoinedEvent>(networkID));
				break;
			}
		}
		break;


		//int offset = 2;  // skip [CMDID] and [EventType], which are the first 2 bytes

		//for (uint8_t i = 0; i < eventData[1]; ++i) {
		//	uint8_t eventType = eventData[offset++];

		//	// 1) Parse the network ID
		//	NetworkID networkID;
		//	std::memcpy(&networkID, &eventData[offset], sizeof(networkID));
		//	networkID = ntohl(networkID);
		//	offset += sizeof(networkID);

		//	switch (eventType) {
		//	case static_cast<uint8_t>(EventType::SpawnPlayer):
		//		// Next we parse the name length + name
		//		// (same structure if you also appended name for SpawnPlayer)
		//	{
		//		uint8_t nameLen = eventData[offset++];
		//		std::string playerName(
		//			eventData.begin() + offset,
		//			eventData.begin() + offset + nameLen
		//		);
		//		offset += nameLen;

		//		// Now you have the player's name. Store it or log it.
		//		NetworkEngine::GetInstance().playerNames[networkID] = playerName;

		//		// Push the normal SpawnPlayerEvent
		//		EventQueue::GetInstance().Push(std::make_unique<SpawnPlayerEvent>(networkID));

		//		// Optional: Print for debugging
		//		std::cout << "[StartGame] SpawnPlayer with ID=" << networkID
		//			<< ", name=" << playerName << std::endl;
		//	}
		//	break;

		//	case static_cast<uint8_t>(EventType::PlayerJoined):
		//	{
		//		// parse nameLen
		//		uint8_t nameLen = eventData[offset++];
		//		// read that many chars from eventData
		//		std::string playerName(
		//			eventData.begin() + offset,
		//			eventData.begin() + offset + nameLen
		//		);
		//		offset += nameLen;

		//		// store or log it
		//		NetworkEngine::GetInstance().playerNames[networkID] = playerName;

		//		EventQueue::GetInstance().Push(std::make_unique<PlayerJoinedEvent>(networkID));
		//		std::cout << "[StartGame] PlayerJoined with ID=" << networkID
		//			<< ", name=" << playerName << std::endl;
		//	}
		//	break;
		//	}
		//}
		//break;

		//int offset = 2;
		//while (offset < eventData.size()) {
		//	uint8_t eventType = eventData[offset++];
		//	NetworkID netID = /* parse networkID */;
		//	uint8_t nameLen = eventData[offset++];
		//	std::string name(eventData.data() + offset, eventData.data() + offset + nameLen);
		//	offset += nameLen;

		//	// Store locally
		//	playerNames[netID] = name;

		//	// Handle spawn event
		//	if (eventType == static_cast<uint8_t>(EventType::PlayerJoined)) {
		//		EventQueue::Push(std::make_unique<PlayerJoinedEvent>(netID));
		//	}
		//}
		//break;
	}
	case EventType::SpawnAsteroid: {

		std::memcpy(&networkID, &eventData[1], sizeof(networkID));
		networkID = ntohl(networkID);

		EventQueue::GetInstance().Push(std::make_unique<SpawnAsteroidEvent>(networkID, eventData));	
		break;
	}
	case EventType::Collision: {
 		int offset = 1;
		NetworkID idA,idB;
		std::memcpy(&idA, &eventData[offset], sizeof(idA));
		std::memcpy(&idB, &eventData[offset + sizeof(idA)], sizeof(idB));
		idA = ntohl(idA);
		idB = ntohl(idB);

		auto it = std::make_unique<CollisionEvent>(idA, idB);
		it->id = networkID;
		EventQueue::GetInstance().Push(std::move(it));
		break;
	}
							  // Add cases for other lockstepped events here...
							  // case EventType::AnotherEvent: { ... break; }
	default:
		std::cerr << "[Client] Cannot process unknown committed event type: " << static_cast<int>(eventType) << std::endl;
		break;
	}


	// Remove the processed event from the pending map
	pendingClientEvents.erase(it);
}


void NetworkEngine::ProcessTickSync(Tick& receivedHostTick, Tick& localTick) {

}

void NetworkEngine::SendFullStateSnapshot(const sockaddr_in& clientAddr) {
	std::vector<char> packet;
	packet.push_back(CMDID::FULL_STATE_SNAPSHOT);

	uint32_t activeCount = std::count_if(g_AsteroidScene->gameObjects.begin(), g_AsteroidScene->gameObjects.end(), [](const std::unique_ptr<GameObject>& obj) {
		return obj->isActive;
	});

	//NetworkUtils::WriteToPacket(packet, activeCount, NetworkUtils::DT_LONG);

	//for (auto& go : g_AsteroidScene->gameObjects) {
	//	if (go->isActive) {
	//		if (go->type == GameObject::GO_BULLET) continue;
	//		packet.push_back(go->type);
	//		auto temp = dynamic_cast<NetworkObject*>(go.get())->Serialize();
	//		packet.insert(packet.end(), temp.begin(), temp.end());
	//	}
	//}

	//socketManager.SendToClient(clientAddr, packet);
}

void NetworkEngine::HandleFullStateSnapshot(const std::vector<char>& data) {
	size_t offset = 1;
	uint32_t totalObjects;
	NetworkUtils::ReadFromPacket(data.data(), offset, totalObjects, NetworkUtils::DT_LONG);
	offset += sizeof(totalObjects);

	for (uint32_t i = 0; i < totalObjects; i++) {
		char objType = data[offset++];

		uint32_t netID;
		NetworkUtils::ReadFromPacket(data.data(), offset, netID, NetworkUtils::DT_LONG);
		offset += sizeof(netID);

		switch (objType) {


		}
	}
}

size_t NetworkEngine::GetNumConnectedClients() const
{
	size_t count = 0;
	for (const auto& client : clientManager.GetClients()) {
		if (client.isConnected) {
			count++;
		}
	}
	return count;
}

void NetworkEngine::CheckTimeoutsAndHeartbeats() {
	if (!isHosting) return;
	
	auto now = std::chrono::steady_clock::now();
	std::vector<sockaddr_in> clientsToDisconnect;
	std::vector<EventID> eventsToTimeout;
			
	//Check Client Heartbeats
	for (auto& client : clientManager.GetClientsNonConst()) { // Need non-const access
		if (!client.isConnected) continue; // Skip already disconnected
		
		auto timeSinceHeartbeat = std::chrono::duration_cast<std::chrono::milliseconds>(now - client.lastHeartbeatTime).count();
		if (timeSinceHeartbeat > CLIENT_TIMEOUT_MS) {
			std::cerr << "[Host] Client ID: " << client.clientID << " timed out (Last Heartbeat: " << timeSinceHeartbeat << "ms ago)." << std::endl;
			client.isConnected = false; // Mark as disconnected
			clientsToDisconnect.push_back(client.address);
			
			// TODO: Broadcast PlayerLeftEvent via lockstep
			// Need a mechanism to inject server-side events into the lockstep flow
			// For now, just mark as disconnected. Other clients won't know yet.
				
		}
		
	}

	// Optionally, remove clients entirely after disconnect handling
	for(sockaddr_in add : clientsToDisconnect) { 
		clientManager.RemoveClient(add);
	}
		
		
	// Check Pending Event Timeouts
	for (auto const& [eventID, pendingInfo] : pendingAcks) {
		auto timeSinceBroadcast = std::chrono::duration_cast<std::chrono::milliseconds>(now - pendingInfo.broadcastTime).count();
		
		if (timeSinceBroadcast > EVENT_TIMEOUT_MS) {
			std::cerr << "[Host] Event ID: " << eventID << " timed out (" << timeSinceBroadcast << "ms). Discarding." << std::endl;
			eventsToTimeout.push_back(eventID);
			// Don't send COMMIT. Clients waiting for it will eventually need their own timeout/cleanup.
				
		}
		
	}
	
	// Remove timed-out events
	for (EventID id : eventsToTimeout) {
		pendingAcks.erase(id);
	}
}

void NetworkEngine::CheckAckTimeouts() 
{
	static constexpr int ACK_TIMEOUT_MS = 3000; // 3 second

	for (auto& [eventID, pendingInfo] : pendingAcks) {
		auto timeSinceBroadcast = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - pendingInfo.broadcastTime).count();
		if (timeSinceBroadcast > ACK_TIMEOUT_MS) {
			for (auto& client : clientManager.GetClients()) {
				if (pendingInfo.acksReceived.find(client.clientID) == pendingInfo.acksReceived.end()) {
					std::cerr << "[Host] ACK timeout for Event ID: " << eventID << " from Client " << client.clientID << std::endl;
					// Resend the event to the client
					SendToClient(client, pendingInfo.eventData);
					pendingInfo.broadcastTime = std::chrono::steady_clock::now(); // Update the broadcast time
				}
			}
		}
	}
}