#include "NetworkEngine.hpp"

#include <iostream>

#include "ws2tcpip.h"		// getaddrinfo()

#include "../Events/EventQueue.hpp"
#include "../AsteroidScene.hpp" // HACK: Include scene for now for state access.

// Tell the Visual Studio linker to include the following library in linking.
// Alternatively, we could add this file to the linker command-line parameters,
// but including it in the source code simplifies the configuration.
#pragma comment(lib, "ws2_32.lib")

#ifndef WINSOCK_VERSION
#define WINSOCK_VERSION     2
#endif
#define WINSOCK_SUBVERSION  2
#define MAX_STR_LEN         1000

extern Tick simulationTick;
extern Tick localTick;

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
				HandleClientEvent(data, sender);
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

		while (socketManager.ReceiveFromHost(data)) {

			

			if (data.empty()) continue;

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
			case GAME_EVENT: {
				int offset = 2;
				for (uint8_t i = 0; i < data[1]; ++i) {
					uint8_t eventType = data[offset++];
					NetworkID networkID;
					std::memcpy(&networkID, &data[offset], sizeof(networkID));
					networkID = ntohl(networkID);
					offset += sizeof(networkID);

					switch (eventType) {
					case static_cast<uint8_t>(EventType::SpawnPlayer):
						EventQueue::GetInstance().Push(std::make_unique<SpawnPlayerEvent>(networkID));
						break;
					case static_cast<uint8_t>(EventType::PlayerJoined):
						EventQueue::GetInstance().Push(std::make_unique<PlayerJoinedEvent>(networkID));
						break;
					case static_cast<uint8_t>(EventType::SpawnAsteroid):
						EventQueue::GetInstance().Push(std::make_unique<SpawnAsteroidEvent>(networkID, data));
						break;
					}
				}
				break;
			}
			case BROADCAST_EVENT: // Host broadcasting an event for lockstep
				HandleBroadcastEvent(data);
				break;
			case COMMIT_EVENT: // Host commanding the client to process a specific event
				HandleCommitEvent(data);
				break;
			case INITIAL_STATE_OBJECT: // Host sending initial state for an object
				//HandleInitialStateObject(data);
				break;
			default:
				// Optional: Log unknown packet type
				break;
			}
		}
	}

#pragma region old stuff
	// Host
	//if (udpListeningSocket != INVALID_SOCKET) {
	//	char buffer[100] = { 0 };
	//	sockaddr_in clientAddr;
	//	int addrLen = sizeof(clientAddr);
	//	int recvResult = recvfrom(
	//		udpListeningSocket, 
	//		buffer, 
	//		sizeof(buffer), 
	//		0, 
	//		reinterpret_cast<sockaddr*>(&clientAddr), 
	//		&addrLen);
	//	if (recvResult > 0) {
	//		if (buffer[0] == REQ_CONNECTION) {
	//			std::vector<char> packet;
	//			CMDID reply = RSP_CONNECTION;
	//			packet.push_back(reply);

	//			int sendResult = sendto(
	//				udpListeningSocket,
	//				packet.data(),
	//				packet.size(), 0,
	//				reinterpret_cast<sockaddr*>(&clientAddr),
	//				addrLen);
	//			if (sendResult == SOCKET_ERROR) {
	//				std::cerr << "sendto() for RSP_CONNECTION failed. Error: " << WSAGetLastError() << "\n";
	//			}
	//			char ipBuffer[INET_ADDRSTRLEN];
	//			inet_ntop(
	//				AF_INET, 
	//				&(clientAddr.sin_addr), 
	//				ipBuffer, 
	//				INET_ADDRSTRLEN);
	//			std::string clientIP(ipBuffer);

	//			bool exists = false;
	//			for (const auto& client : clientConnections) {
	//				if (client.ipAddress == clientIP && ntohs(clientAddr.sin_port) == client.udpPort) {
	//					exists = true;
	//					break;
	//				}
	//			}

	//			if (!exists) {
	//				Client newClient;
	//				newClient.address = clientAddr;
	//				newClient.ipAddress = clientIP;
	//				newClient.udpPort = ntohs(clientAddr.sin_port);
	//				newClient.isConnected = true;
	//				clientConnections.push_back(newClient);
	//				std::cout << "Added new client: " << clientIP << ":" << newClient.udpPort << "\n";
	//				EventQueue::GetInstance().Push(std::make_unique<ClientJoinedEvent>());
	//			}
	//		} else if (buffer[0] == GAME_DATA) {
	//			std::cout << "GAME DATA RECEIVED\n";
	//			//float test1 = static_cast<float>(static_cast<int8_t>(buffer[1]));
	//			//float test2 = static_cast<float>(static_cast<int8_t>(buffer[2]));
	//			//std::cout << test1 << " :BUFFER: " << test2 << std::endl;
	//			EventQueue::GetInstance().Push(std::make_unique<PlayerUpdate>(buffer, recvResult));
	//		}
	//	}
	//} else if (clientUDPSocket != INVALID_SOCKET) { // Client
	//	char buffer[1472];
	//	int addrLen = sizeof(serverInfo.address);
	//	int recvResult = recvfrom(
	//		clientUDPSocket,
	//		buffer,
	//		sizeof(buffer),
	//		0,
	//		reinterpret_cast<sockaddr*>(&serverInfo.address),
	//		&addrLen);

	//	if (recvResult > 0) {
	//		if (buffer[0] == TICK_SYNC) {
	//			//std::cout << "TICK FROM SERVER RECEIVED\n";
	//			Tick receivedTick;
	//			std::memcpy(&receivedTick, buffer + 1, sizeof(receivedTick));
	//			localTick = ntohl(receivedTick);
	//		} else if (buffer[0] == GAME_DATA) {

	//		}
	//	}
	//}
#pragma endregion
}


bool NetworkEngine::Host(std::string portNumber) {
	isHosting = socketManager.Host(portNumber);

	if (isHosting) {
		isClient = false; 
		std::cout << "Hosting on IP: " << GetIPAddress() << " Port: " << portNumber << std::endl;
	}
	return isHosting;
}

bool NetworkEngine::Connect(std::string host, std::string portNumber) {
	isClient = socketManager.ConnectWithHandshake(host, portNumber,
		CMDID::REQ_CONNECTION, CMDID::RSP_CONNECTION);

	if (isClient) {
		isHosting = false;
		std::cout << "Connected to host: " << host << " Port: " << portNumber << std::endl;
	}

	return isClient;
}

void NetworkEngine::Exit() {
#pragma region old stuff


	// client cleanup
	//if (clientUDPSocket != INVALID_SOCKET) {
	//	closesocket(clientUDPSocket);
	//	clientUDPSocket = INVALID_SOCKET;
	//}

	//// host cleanup
	//if (udpListeningSocket != INVALID_SOCKET) {
	//	closesocket(udpListeningSocket);
	//	udpListeningSocket = INVALID_SOCKET;
	//	clientConnections.clear();
	//}
#pragma endregion
	
	socketManager.Cleanup();
	WSACleanup();
}

// Client function to send an event to the server for lockstep processing
void NetworkEngine::SendEventToServer(std::unique_ptr<GameEvent> eventt) {
	if (!isClient) return;

	std::vector<char> packet;
	packet.push_back(CMDID::GAME_EVENT); // Mark as client-submitted event
	packet.push_back(static_cast<char>(eventt->type)); // Add the event type

	// Serialize the specific event data
	if (eventt->type == EventType::FireBullet) {
		// Assuming FireBulletEvent has a Serialize method or we do it here
		auto fireEvent = static_cast<FireBulletEvent*>(eventt.get());
		std::vector<char> eventData = fireEvent->Serialize();
		packet.insert(packet.end(), eventData.begin(), eventData.end());
	}
	// Add other event types here...
	// else if (event->type == ...) { ... }


	if (packet.size() > 2) { // Ensure we actually added event data
		socketManager.SendToHost(packet);
	}
	else {
		std::cerr << "Warning: Tried to send unknown or empty event type: " << static_cast<int>(eventt->type) << std::endl;
	}
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
	if (data.empty() || data[0] != REQ_CONNECTION) return;

	if (!clientManager.IsKnownClient(clientAddr)) {
		clientManager.AddClient(clientAddr);
		//EventQueue::GetInstance().Push(std::make_unique<ClientJoinedEvent>());

		auto newClientOpt = clientManager.GetClientByAddr(clientAddr);
		if (newClientOpt) {
			socketManager.SendToClient(clientAddr, static_cast<char>(RSP_CONNECTION)); // Send ACK first
			//SendInitialState(newClientOpt.value().get()); // Then send current state

		}
	}
}

void NetworkEngine::HandleClientEvent(const std::vector<char>& data, const sockaddr_in& clientAddr) {
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
		std::cout << "[Host] Received ACK for Event ID: " << eventID << " from Client " << senderClientID
			<< " (" << pendingInfo.acksReceived.size() << "/" << clientManager.GetClients().size() << ")" << std::endl;
	}


	// Check if all connected clients have ACKed
	if (pendingInfo.acksReceived.size() >= GetNumConnectedClients()) {
		std::cout << "[Host] All ACKs received for Event ID: " << eventID << ". Committing." << std::endl;

		// Prepare commit packet
		std::vector<char> commitPacket;
		commitPacket.push_back(CMDID::COMMIT_EVENT);
		EventID netEventID = htonl(eventID);
		commitPacket.insert(commitPacket.end(), reinterpret_cast<char*>(&netEventID), reinterpret_cast<char*>(&netEventID) + sizeof(netEventID));

		// Send commit command to all clients
		SendToAllClients(commitPacket);

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

	std::cout << "[Client] Received BROADCAST_EVENT (Type: " << static_cast<int>(eventType) << ") ID: " << eventID << std::endl;


	// Store the event data (excluding CMDID and EventID) for later processing
	// Start copying after the EventID
	pendingClientEvents[eventID].assign(data.begin() + 1 + sizeof(EventID), data.end());

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


		EventQueue::GetInstance().Push(std::make_unique<FireBulletEvent>(pos, rot, ownerId));
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

//void NetworkEngine::SendPacket(std::vector<char> packet) {
//	if (clientUDPSocket != INVALID_SOCKET) {
//		int sendResult = sendto(
//			clientUDPSocket,
//			packet.data(),
//			static_cast<int>(packet.size()), 
//			0,
//			reinterpret_cast<sockaddr*>(&serverInfo.address),
//			sizeof(serverInfo.address));
//	}
//}
