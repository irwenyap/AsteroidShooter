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

	if (isHosting && socketManager.ReceiveFromClient(data, sender)) {
		switch (data[0]) {
		case REQ_CONNECTION:
			HandleIncomingConnection(data, sender);
			break;
		case GAME_DATA:
			EventQueue::GetInstance().Push(std::make_unique<PlayerUpdate>(data.data(), data.size()));
			SendToOtherClients(sender, data);
			break;
		case GAME_EVENT:
			break;
		default:
			break;
		}
	} else if (isClient) {
		//switch (data[0]) {
		//case TICK_SYNC: {
		//	Tick receivedTick;
		//	std::memcpy(&receivedTick, &data[1], sizeof(receivedTick));
		//	localTick = ntohl(receivedTick);
		//	break;
		//}
		//case GAME_DATA: {
		//	// handle later
		//	EventQueue::GetInstance().Push(std::make_unique<PlayerUpdate>(data.data(), data.size()));
		//	break;
		//}
		//case GAME_EVENT: {
		//	// process all events
		//	int offset = 2;
		//	for (uint8_t i = 0; i < data[1]; ++i) {
		//		//if (data[offset] == static_cast<char>(EventType::SpawnPlayer)) {
		//		//	NetworkID networkID;
		//		//	std::memcpy(&networkID, &data[offset], sizeof(networkID));
		//		//	networkID = ntohl(networkID);
		//		//	EventQueue::GetInstance().Push(std::make_unique<SpawnPlayerEvent>(networkID));
		//		//	offset += 5;
		//		//} else if (data[offset] == static_cast<char>(EventType::ConnectedPlayer)) {
		//		//	NetworkID networkID;
		//		//	std::memcpy(&networkID, &data[offset], sizeof(networkID));
		//		//	networkID = ntohl(networkID);
		//		//	EventQueue::GetInstance().Push(std::make_unique<ConnectedPlayerEvent>(networkID));
		//		//	offset += 5;
		//		//}
		//		uint8_t eventType = data[offset];
		//		offset += 1;

		//		NetworkID networkID;
		//		std::memcpy(&networkID, &data[offset], sizeof(networkID));
		//		networkID = ntohl(networkID);
		//		offset += sizeof(networkID);

		//		switch (eventType) {
		//		case static_cast<uint8_t>(EventType::SpawnPlayer):
		//			EventQueue::GetInstance().Push(std::make_unique<SpawnPlayerEvent>(networkID));
		//			break;
		//		case static_cast<uint8_t>(EventType::ConnectedPlayer):
		//			EventQueue::GetInstance().Push(std::make_unique<ConnectedPlayerEvent>(networkID));
		//			break;
		//		case static_cast<uint8_t>(EventType::SpawnAsteroid):
		//			EventQueue::GetInstance().Push(std::make_unique<SpawnAsteroidEvent>(networkID, data));
		//			break;
		//		}
		//	}
		//}
		//}
		while (socketManager.ReceiveFromHost(data)) {
			switch (data[0]) {
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
					case static_cast<uint8_t>(EventType::ConnectedPlayer):
						EventQueue::GetInstance().Push(std::make_unique<ConnectedPlayerEvent>(networkID));
						break;
					case static_cast<uint8_t>(EventType::SpawnAsteroid):
						EventQueue::GetInstance().Push(std::make_unique<SpawnAsteroidEvent>(networkID, data));
						break;
					case static_cast<uint8_t>(EventType::ReqFireBullet):
						EventQueue::GetInstance().Push(std::make_unique<ReqFireBulletEvent>(data));
						break;

					}
				}
				break;
			}
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
	return socketManager.Host(portNumber);
}
static uint32_t myPlayerID; // temp
bool NetworkEngine::Connect(std::string host, std::string portNumber) {
#pragma region old stuff
	//addrinfo tcpHints{};
	//SecureZeroMemory(&tcpHints, sizeof(tcpHints));
	//tcpHints.ai_family = AF_INET;
	//tcpHints.ai_socktype = SOCK_STREAM;
	//tcpHints.ai_protocol = IPPROTO_TCP;

	//addrinfo* tcpInfo = nullptr;
	//int errorCode = getaddrinfo(host.c_str(), portNumber.c_str(), &tcpHints, &tcpInfo);
	//if ((NO_ERROR != errorCode) || (nullptr == tcpInfo)) {
	//	std::cerr << "getaddrinfo() failed for TCP." << std::endl;
	//	WSACleanup();
	//	return false;
	//}

	//clientTCPSocket = socket(
	//	tcpInfo->ai_family,
	//	tcpInfo->ai_socktype,
	//	tcpInfo->ai_protocol);
	//if (INVALID_SOCKET == clientTCPSocket) {
	//	std::cerr << "socket() failed." << std::endl;
	//	freeaddrinfo(tcpInfo);
	//	WSACleanup();
	//	return false;
	//}

	//errorCode = connect(
	//	clientTCPSocket,
	//	tcpInfo->ai_addr,
	//	static_cast<int>(tcpInfo->ai_addrlen));
	//if (SOCKET_ERROR == errorCode) {
	//	std::cerr << "connect() failed." << std::endl;
	//	freeaddrinfo(tcpInfo);
	//	closesocket(clientTCPSocket);
	//	WSACleanup();
	//	return false;
	//}


	//addrinfo udpHints{};
	//SecureZeroMemory(&udpHints, sizeof(udpHints));
	//udpHints.ai_family = AF_INET;
	//udpHints.ai_socktype = SOCK_DGRAM;
	//udpHints.ai_protocol = IPPROTO_UDP;

	//addrinfo* udpInfo = nullptr;
	//int errorCode = getaddrinfo(host.c_str(), portNumber.c_str(), &udpHints, &udpInfo);
	//if ((NO_ERROR != errorCode) || (nullptr == udpInfo)) {
	//	std::cerr << "getaddrinfo() failed for UDP. Error code: " << errorCode << "\n";
	//	WSACleanup();
	//	return false;
	//}

	//clientUDPSocket = socket(
	//	udpInfo->ai_family,
	//	udpInfo->ai_socktype,
	//	udpInfo->ai_protocol);
	//if (INVALID_SOCKET == clientUDPSocket) {
	//	std::cerr << "socket() failed for UDP.\n";
	//	freeaddrinfo(udpInfo);
	//	WSACleanup();
	//	return false;
	//}

	//const int maxRetries = 5;
	//int retryCount = 0;
	//bool connectionEstablished = false;
	//CMDID cmd = REQ_CONNECTION;
	//fd_set readfds;
	//timeval timeout = { 1, 0 }; // 1 second timeout for each attempt

	//while (!connectionEstablished && retryCount < maxRetries) {
	//	// send REQ_CONNECTION to the server
	//	int sendResult = sendto(clientUDPSocket, reinterpret_cast<char*>(&cmd), sizeof(cmd), 0, udpInfo->ai_addr, (int)udpInfo->ai_addrlen);
	//	if (sendResult == SOCKET_ERROR) {
	//		std::cerr << "sendto() failed. Error code: " << WSAGetLastError() << "\n";
	//		freeaddrinfo(udpInfo);
	//		closesocket(clientUDPSocket);
	//		WSACleanup();
	//		return false;
	//	}

	//	FD_ZERO(&readfds);
	//	FD_SET(clientUDPSocket, &readfds);
	//	int selResult = select(0, &readfds, NULL, NULL, &timeout);
	//	if (selResult > 0 && FD_ISSET(clientUDPSocket, &readfds)) {
	//		//CMDID response = UNKNOWN;
	//		char buffer[100];
	//		sockaddr_in serverAddr;
	//		int addrLen = sizeof(serverAddr);
	//		int recvResult = recvfrom(clientUDPSocket, buffer, sizeof(buffer), 0, reinterpret_cast<sockaddr*>(&serverAddr), &addrLen);
	//		if (recvResult > 0 && buffer[0] == RSP_CONNECTION) {
	//			connectionEstablished = true;
	//			//std::memcpy(&myPlayerID, my)
	//			serverInfo.address = serverAddr;
	//			std::cout << "Received RSP_CONNECTION from server.\n";
	//			break;
	//		}
	//	}
	//	retryCount++;
	//	std::cout << "Retry " << retryCount << "...\n";
	//}
	//freeaddrinfo(udpInfo);

	//if (!connectionEstablished) {
	//	std::cerr << "Failed to establish connection after retries.\n";
	//	closesocket(clientUDPSocket);
	//	clientUDPSocket = INVALID_SOCKET;
	//	return false;
	//}

	//u_long nonBlocking = 1;
	//ioctlsocket(clientUDPSocket, FIONBIO, &nonBlocking);

	//return true;
#pragma endregion
	return socketManager.ConnectWithHandshake(host, portNumber, 
		CMDID::REQ_CONNECTION, CMDID::RSP_CONNECTION);
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

void NetworkEngine::SendToHost(const std::vector<char>& data)
{
	socketManager.SendToHost(data);
}