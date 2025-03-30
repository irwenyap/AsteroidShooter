#include "NetworkEngine.hpp"
#include <iostream>
#include "ws2tcpip.h"		// getaddrinfo()
#include "../Events/EventQueue.hpp"

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


// Define mapping from EventType to ChannelId
ChannelId NetworkEngine::GetChannelForEventType(EventType type) {
	switch (type) {
	case EventType::RequestStartGame:
	case EventType::StartGame:
	case EventType::PlayerJoined:
	case EventType::PlayerLeft:
	case EventType::SpawnPlayer:
	case EventType::SpawnAsteroid:
	case EventType::FireBullet: 
		return ChannelId::RELIABLE_ORDERED;
	case EventType::PlayerUpdate:
		// Client -> Host often uses unreliable, Host -> Clients might too
		return ChannelId::UNRELIABLE_ORDERED;
	default:
		return ChannelId::RELIABLE_ORDERED; // Default reliable
	}
}

// Helper to hash sockaddr_in for map key
struct SockAddrInHasher {
	std::size_t operator()(const sockaddr_in& addr) const {
		size_t h1 = std::hash<uint32_t>{}(addr.sin_addr.s_addr);
		size_t h2 = std::hash<uint16_t>{}(addr.sin_port);
		return h1 ^ (h2 << 1);
	}
};
struct SockAddrInEqual {
	bool operator()(const sockaddr_in& lhs, const sockaddr_in& rhs) const {
		return lhs.sin_addr.s_addr == rhs.sin_addr.s_addr && lhs.sin_port == rhs.sin_port;
	}
};



NetworkEngine& NetworkEngine::GetInstance() {
	static NetworkEngine ne;
	return ne;
}

void NetworkEngine::Initialize() {

	if (isInitialized) return;

	WSADATA wsaData{};
	SecureZeroMemory(&wsaData, sizeof(wsaData));

	int errorCode = WSAStartup(MAKEWORD(WINSOCK_VERSION, WINSOCK_SUBVERSION), &wsaData);
	if (NO_ERROR != errorCode) {
		std::cerr << "WSAStartup() failed.\n";
		return;
	}

	// Initialize Channel Configurations
	channelConfigs[ChannelId::RELIABLE_ORDERED] = { true, true};
	channelConfigs[ChannelId::UNRELIABLE_ORDERED] = { false, true};
	channelConfigs[ChannelId::ACKNACK_RELIABLE] = { true, false}; // Reliable, Unordered
	channelConfigs[ChannelId::COMMIT_TICK_RELIABLE] = { true, false}; // Reliable, Unordered

	isInitialized = true;
	std::cout << "Network Engine Initialized.\n";
}

bool NetworkEngine::Host(std::string port) {
	if (!isInitialized || isHosting || isClient) return false;
	if (!socketManager.Host(port)) {
		std::cerr << "NetworkEngine::Host failed to setup socket.\n";
		return false;
	}
	isHosting = true;
	myNetworkId = 0; // Host is often ID 0 or a special value
	currentNetworkTick = 0;
	lastCommittedTick = 0;
	portNumber = port;
	std::cout << "Hosting on IP: " << GetIPAddress() << " Port: " << port << std::endl;
	return true;
}


bool NetworkEngine::Connect(std::string hostIp, std::string port) {
	if (!isInitialized || isHosting || isClient || isConnecting) return false;

	if (!socketManager.Connect(hostIp, port)) {
		std::cerr << "NetworkEngine::Connect failed to create socket.\n";
		return false;
	}

	isConnecting = true;
	isClient = true; // Assume client role now
	hostAddress = socketManager.GetServerAddress(); // Get resolved address

	// Send REQ_CONNECTION reliably (using a basic retry for handshake)
	const int maxRetries = 5;
	int retryCount = 0;
	bool ackReceived = false;
	std::vector<char> reqPacket = { CMDID::REQ_CONNECTION }; // Simple request payload
	timeval timeout = { 1, 0 }; // 1 second timeout

	std::cout << "Attempting connection to " << hostIp << ":" << port << "...\n";

	while (!ackReceived && retryCount < maxRetries) {
		socketManager.SendToHost(reqPacket); // Raw send for handshake

		fd_set readfds;
		FD_ZERO(&readfds);
		FD_SET(socketManager.GetClientSocket(), &readfds); // Use socket from manager

		int selResult = select(0, &readfds, nullptr, nullptr, &timeout);

		if (selResult > 0 && FD_ISSET(socketManager.GetClientSocket(), &readfds)) {
			std::vector<char> recvBuffer(1500);
			sockaddr_in senderAddr;
			int addrLen = sizeof(senderAddr);
			int bytesRead = recvfrom(socketManager.GetClientSocket(), recvBuffer.data(), recvBuffer.size(), 0, (SOCKADDR*)&senderAddr, &addrLen);

			// Check if it's from the host we expect and is the RSP_CONNECTION
			if (bytesRead > 0 && senderAddr.sin_addr.s_addr == hostAddress.sin_addr.s_addr && senderAddr.sin_port == hostAddress.sin_port)
			{
				if (bytesRead >= (sizeof(PacketHeader) + sizeof(NetworkID)) && recvBuffer[0] == static_cast<uint8_t>(ChannelId::RELIABLE_ORDERED)) {
					// Proper response likely comes on reliable channel now
					PacketHeader* header = reinterpret_cast<PacketHeader*>(recvBuffer.data());
					char* payload = recvBuffer.data() + sizeof(PacketHeader);
					size_t payloadSize = bytesRead - sizeof(PacketHeader);

					if (payloadSize >= sizeof(NetworkID)) {
						// Assuming payload[0] implicitly indicates RSP_CONNECTION
						std::memcpy(&myNetworkId, payload, sizeof(NetworkID));
						myNetworkId = ntohl(myNetworkId); // Get assigned ID
						ackReceived = true;
						std::cout << "Connection successful! Assigned NetworkID: " << myNetworkId << std::endl;
					}
				}
				//else if (bytesRead == 1 && recvBuffer[0] == CMDID::RSP_CONNECTION) {
				//	// Fallback for simple ACK if host hasn't switched to channels yet
				//	ackReceived = true;
				//	myNetworkId = 1; // Assign a default? Host should send ID.
				//	std::cerr << "Warning: Received simple connection ACK. Host should send NetworkID.\n";
				//}
			}
		}
		else if (selResult == 0) {
			std::cout << "Connection attempt timed out. Retrying (" << retryCount + 1 << "/" << maxRetries << ")...\n";
		}
		else {
			std::cerr << "Select error during connection: " << WSAGetLastError() << std::endl;
			break; // Socket error
		}
		retryCount++;
	}

	isConnecting = false;
	if (ackReceived) {
		isConnected = true;
		socketManager.SetNonBlocking(socketManager.GetClientSocket());
		return true;
	}
	else {
		std::cerr << "Connection failed after " << maxRetries << " retries.\n";
		socketManager.Cleanup(); // Close the socket
		isClient = false;
		return false;
	}
}


void NetworkEngine::Update(double dt) {
	if (!isInitialized || (!isHosting && !isClient)) return;

	ProcessIncomingPackets();

	if (isHosting) {
		UpdateCommitTick(dt);
		CheckTimeouts(dt);
		currentNetworkTick++; // Advance host tick
	}
	else if (isClient && isConnected) {
		// Client-specific updates (e.g., sending input) happen elsewhere via SendToServer
	}
}

void NetworkEngine::ProcessIncomingPackets() {
	std::vector<char> recvBuffer(1500); // Max UDP payload size
	sockaddr_in senderAddr;

	SOCKET currentSocket = isHosting ? socketManager.GetHostSocket() : socketManager.GetClientSocket();
	if (currentSocket == INVALID_SOCKET) return;

	int bytesRead;
	do {
		bytesRead = socketManager.Receive(currentSocket, recvBuffer.data(), recvBuffer.size(), senderAddr);
		if (bytesRead > 0) {
			HandlePacket(recvBuffer.data(), bytesRead, senderAddr);
		}
		else if (bytesRead == SOCKET_ERROR) {
			int error = WSAGetLastError();
			if (error != WSAEWOULDBLOCK && error != WSAECONNRESET) { // Ignore WouldBlock, handle reset
				std::cerr << "recvfrom error: " << error << std::endl;
				// Potentially handle disconnect
			}
		}
	} while (bytesRead > 0); // Keep processing until WSAEWOULDBLOCK
}


void NetworkEngine::HandlePacket(const char* buffer, int bytesRead, const sockaddr_in& senderAddr) {

	if (isHosting) {

		// --- Host Packet Handling ---
		uint64_t addrHash = SockAddrInHasher{}(senderAddr);
		auto mapIt = addrToClientIdMap.find(addrHash);

		// Check for initial connection request (doesn't use channels/headers yet)
		if (bytesRead == 1 && buffer[0] == CMDID::REQ_CONNECTION) {
			std::cout << "Received REQ_CONNECTION from new potential client.\n";
			NetworkID newClientId = nextNetworkId++;
			ClientNetworkState newClientState;
			newClientState.address = senderAddr;
			newClientState.clientId = newClientId;
			newClientState.isConnected = true; // Mark as connected after response
			newClientState.lastHeardFrom = std::chrono::steady_clock::now();
			connectedClients[newClientId] = newClientState;


			// Send RSP_CONNECTION back with their assigned ID on a reliable channel
			std::vector<char> payload;
			NetworkID netId = htonl(newClientId);
			payload.insert(payload.end(), reinterpret_cast<char*>(&netId), reinterpret_cast<char*>(&netId) + sizeof(netId));

			// Use InternalSend to add header and handle potential retransmission
			InternalSend(socketManager.GetHostSocket(), senderAddr, ChannelId::RELIABLE_ORDERED,
				connectedClients[newClientId].nextSequenceToSend[ChannelId::RELIABLE_ORDERED]++, // Get & increment seq
				payload, true);

			std::cout << "Sent RSP_CONNECTION with ID " << newClientId << " to client.\n";
			// TODO: Trigger PlayerJoined event for other clients? Or wait for game start?

			return; // Handled connection request
		}

		if (mapIt == addrToClientIdMap.end()) {
			std::cerr << "Received packet from unknown address (not REQ_CONNECTION). Ignoring.\n";
			return; // Ignore packets from clients not in our map (unless it's REQ_CONN)
		}

		NetworkID clientId = mapIt->second;
		ClientNetworkState& clientState = connectedClients[clientId];
		clientState.lastHeardFrom = std::chrono::steady_clock::now();

		// All further communication MUST have a header
		if (bytesRead < sizeof(PacketHeader)) {
			std::cerr << "Received packet too small for header from client " << clientId << ". Ignoring.\n";
			return;
		}

		PacketHeader* header = (PacketHeader*)buffer;
		ChannelId channel = static_cast<ChannelId>(header->channelId);
		SequenceNumber sequence = ntohl(header->sequenceNumber); // Assuming client sends network byte order
		const char* payload = buffer + sizeof(PacketHeader);
		size_t payloadSize = bytesRead - sizeof(PacketHeader);

		HandleClientPacket(clientState, channel, sequence, payload, payloadSize);

	}
	else if (isClient && isConnected) {
		// Client Packet Handling
		// Only accept packets from the known host address
		if (senderAddr.sin_addr.s_addr != hostAddress.sin_addr.s_addr || senderAddr.sin_port != hostAddress.sin_port) {
			std::cerr << "Received packet from unexpected address. Ignoring.\n";
			return;
		}

		if (bytesRead < sizeof(PacketHeader)) {
			std::cerr << "Received packet too small for header from host. Ignoring.\n";
			return;
		}

		PacketHeader* header = (PacketHeader*)buffer;
		ChannelId channel = static_cast<ChannelId>(header->channelId);
		SequenceNumber sequence = ntohl(header->sequenceNumber); // Host sends network byte order
		const char* payload = buffer + sizeof(PacketHeader);
		size_t payloadSize = bytesRead - sizeof(PacketHeader);

		HandleHostPacket(channel, sequence, payload, payloadSize);
	}
}

// Host handles packet received FROM a specific client
void NetworkEngine::HandleClientPacket(ClientNetworkState& clientState, ChannelId channel, SequenceNumber sequence, const char* payload, size_t payloadSize) {
	// Basic validation
	if (channel >= ChannelId::MAX_CHANNELS) {
		std::cerr << "Invalid Channel ID " << static_cast<int>(channel) << " from client " << clientState.clientId << std::endl;
		return;
	}
	const auto& config = channelConfigs.at(channel); // Use .at() for bounds check

	// --- NACK Channel ---
	if (channel == ChannelId::ACKNACK_RELIABLE) {
		HandleNackPacket(payload, payloadSize, clientState);
		// TODO: Handle potential ACKs if using them
		return;
	}

	// --- Sequence Number Handling (for packets FROM client) ---
	// This part is important if clients send reliable/ordered data (like input)
	// Simplified example: Assume only PlayerUpdate is received for now
	if (config.isReliable) {
		// Implement sequence checking similar to HandleHostPacket if needed
	}

	if (config.isSequenced && !config.isReliable) { // e.g., PlayerUpdate
		ClientChannelState& chanState = clientState.receiveState[channel]; // Track received seq from client
		if (sequence > chanState.lastReceivedSequence) {
			chanState.lastReceivedSequence = sequence;
			// Process PlayerUpdate - directly or via queue
			// Assuming PlayerUpdate includes NetworkID
			if (payloadSize > sizeof(NetworkID)) {
				NetworkID senderId;
				memcpy(&senderId, payload, sizeof(NetworkID)); // Assuming ID is first in payload
				senderId = ntohl(senderId);

				if (senderId != clientState.clientId) {
					std::cerr << "Warning: Client " << clientState.clientId << " sent update for ID " << senderId << std::endl;
					// Ignore or handle appropriately
				}
				else {
					// Create event or directly update state
					std::vector<char> updateData(payload, payload + payloadSize);
					EventQueue::GetInstance().Push(std::make_unique<PlayerUpdate>(updateData)); // Push raw data for now

					// --- Broadcast to other clients ---
					// Prepend header for broadcasting
					std::vector<char> broadcastPayload;
					broadcastPayload.push_back(NetworkEngine::CMDID::GAME_DATA); // Old style, maybe change
					// Need to re-serialize with correct structure if broadcasting PlayerUpdate event
					broadcastPayload.insert(broadcastPayload.end(), updateData.begin(), updateData.end());

					// Use the appropriate channel for broadcasting updates
					Broadcast(ChannelId::UNRELIABLE_ORDERED, broadcastPayload, clientState.clientId);
				}
			}
		}
		else {
			// Old sequence, discard
		}
	}
	else if (channel == ChannelId::RELIABLE_ORDERED) {
		// Example: Client sent PlayerInput command reliably/ordered
		// Implement full reliable/ordered receive logic here for client input processing
		// Queue the input command associated with clientState.clientId for processing in TickSimulation
	}
	else {
		std::cout << "Host received unhandled packet type on channel " << static_cast<int>(channel) << " from client " << clientState.clientId << std::endl;
	}
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

void NetworkEngine::SendToAllClients(std::vector<char> packet)
{
	for (auto& client : clientManager.GetClients()) {
		socketManager.SendToClient(client.address, packet);
	}
}

void NetworkEngine::SendToOtherClients(const sockaddr_in& reqClient, std::vector<char> packet)
{
	for (auto& client : clientManager.GetClients()) {
		if (client.address.sin_port == reqClient.sin_port) continue;

		socketManager.SendToClient(client.address, packet);
	}
}

void NetworkEngine::HandleIncomingConnection(const std::vector<char>& data, const sockaddr_in& clientAddr)
{
	if (data.empty() || data[0] != REQ_CONNECTION) return;

	if (!clientManager.IsKnownClient(clientAddr)) {
		clientManager.AddClient(clientAddr);
		//EventQueue::GetInstance().Push(std::make_unique<ClientJoinedEvent>());
	}

	socketManager.SendToClient(clientAddr, static_cast<char>(RSP_CONNECTION));
}

//void NetworkEngine::SendTickSync(Tick& tick) {
//	if (clientConnections.size() == 0) return;
//
//	std::vector<char> packet;
//	packet.push_back(TICK_SYNC);
//	Tick networkSimTick = htonl(tick);
//	packet.insert(packet.end(), reinterpret_cast<char*>(&networkSimTick),
//		reinterpret_cast<char*>(&networkSimTick) + sizeof(networkSimTick));
//	for (auto& client : clientConnections) {
//		int addrLength = sizeof(client.address);
//		int sendResult = sendto(
//			udpListeningSocket,
//			packet.data(),
//			packet.size(), 0,
//			reinterpret_cast<sockaddr*>(&client.address),
//			addrLength);
//		//std::cout << "Sending Tick: " << simulationTick << std::endl;
//	}
//}

void NetworkEngine::ProcessTickSync(Tick& receivedHostTick, Tick& localTick) {

}

size_t NetworkEngine::GetNumConnectedClients() const
{
	return clientManager.GetClients().size();
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
