#include "AsteroidScene.hpp"
#include <random>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>
#include "Graphics/GraphicsEngine.hpp"
#include "Events/EventQueue.hpp"
#include "Player.hpp"
#include "PlayerBullet.hpp"
#include "Networking/NetworkEngine.hpp"
#include <iostream>
#include "Asteroid.hpp"

#define MAX_LOCAL_GAMEOBJECTS 1250
const double asteroidSpawnRate = 5.0;
double asteroidSpawnTimer = 0.0;
bool gameStarted = false;


void AsteroidScene::Initialize() {
	GraphicsEngine::GetInstance().Init();
	gameObjects.reserve(MAX_LOCAL_GAMEOBJECTS);
}

void AsteroidScene::Update(double dt) {
	//for (auto& go : gameObjects) {
	//	go->Update(dt);
	//}
	if (gameStarted && NetworkEngine::GetInstance().isHosting) {
		asteroidSpawnTimer += dt;
		//std::cout << asteroidSpawnTimer << std::endl;
		if (asteroidSpawnTimer >= asteroidSpawnRate) {
			static std::random_device rd;
			static std::mt19937 gen(rd());

			std::uniform_real_distribution<float> posXDist(-20.f, 20.f);
			std::uniform_real_distribution<float> posYDist(-15.f, 15.f);

			std::uniform_real_distribution<float> scaleDist(3.5f, 7.0f);

			std::uniform_real_distribution<float> velocityX(-2.f, 2.f);
			std::uniform_real_distribution<float> velocityY(-2.f, 2.f);

			auto asteroid = std::make_unique<Asteroid>();
			asteroid->networkID = NetworkEngine::GetInstance().GenerateID();
			asteroid->position = glm::vec3(posXDist(gen), posYDist(gen), 0.f);
			float randomScale = scaleDist(gen);
			asteroid->scale = glm::vec3(randomScale, randomScale, 1.f);
			asteroid->velocity = glm::vec3(velocityX(gen), velocityY(gen), 0.f);
			asteroid->rotation = 0.f;
			asteroid->meshType = Mesh::MESH_TYPE::QUAD;
			asteroid->textured = true;
			asteroid->textureType = Texture::TEXTURE_TYPE::TEX_ASTEROID;

			std::vector<char> packet;
			packet.push_back(NetworkEngine::CMDID::GAME_EVENT);
			packet.push_back(static_cast<char>(1));
			packet.push_back(static_cast<char>(EventType::SpawnAsteroid));

			NetworkID netNID = htonl(asteroid->networkID);
			packet.insert(packet.end(), reinterpret_cast<char*>(&netNID),
				reinterpret_cast<char*>(&netNID) + sizeof(netNID));

			int16_t posX = static_cast<int16_t>(asteroid->position.x * 100);
			int16_t posY = static_cast<int16_t>(asteroid->position.y * 100);
			posX = htons(posX);
			posY = htons(posY);
			packet.insert(packet.end(), reinterpret_cast<char*>(&posX),
				reinterpret_cast<char*>(&posX) + sizeof(posX));
			packet.insert(packet.end(), reinterpret_cast<char*>(&posY),
				reinterpret_cast<char*>(&posY) + sizeof(posY));

			int16_t scaleX = static_cast<int16_t>(asteroid->scale.x * 100);
			int16_t scaleY = static_cast<int16_t>(asteroid->scale.y * 100);
			scaleX = htons(scaleX);
			scaleY = htons(scaleY);
			packet.insert(packet.end(), reinterpret_cast<char*>(&scaleX),
				reinterpret_cast<char*>(&scaleX) + sizeof(scaleX));
			packet.insert(packet.end(), reinterpret_cast<char*>(&scaleY),
				reinterpret_cast<char*>(&scaleY) + sizeof(scaleY));


			int16_t velX = static_cast<int16_t>(asteroid->velocity.x * 100);
			int16_t velY = static_cast<int16_t>(asteroid->velocity.y * 100);
			velX = htons(velX);
			velY = htons(velY);
			packet.insert(packet.end(), reinterpret_cast<char*>(&velX),
				reinterpret_cast<char*>(&velX) + sizeof(velX));
			packet.insert(packet.end(), reinterpret_cast<char*>(&velY),
				reinterpret_cast<char*>(&velY) + sizeof(velY));

			Asteroid* rawAsteroid = asteroid.get();
			gameObjects.push_back(std::move(asteroid));
			networkedObjects[rawAsteroid->networkID] = rawAsteroid;

			NetworkEngine::GetInstance().SendToAllClients(packet);
			std::cout << "Server asteroid spawned at: " << rawAsteroid->position.x << ", " << rawAsteroid->position.y << "\n";
			asteroidSpawnTimer = 0.0;
		}
	}
}

void AsteroidScene::FixedUpdate(double fixedDT) {
	for (auto& go : gameObjects) {
		go->Update(fixedDT);
	}
}

void AsteroidScene::ProcessEvents() {
	for (auto& event : EventQueue::GetInstance().Drain()) {
		switch (event->type) {
		case EventType::FireBullet: {
			auto* fire = static_cast<FireBulletEvent*>(event.get());
			glm::vec3 dir(cos(fire->rotation), sin(fire->rotation), 0.f);
			auto bullet = std::make_unique<PlayerBullet>(fire->position, dir);
			//bullet->id = NextID();
			gameObjects.push_back(std::move(bullet));

			// network engine need to broadcast all these events
			break;
		}
		case EventType::StartGame: {
			std::vector<char> packet;
			packet.push_back(NetworkEngine::CMDID::GAME_EVENT);
			// push number of events;
			packet.push_back(static_cast<uint8_t>(NetworkEngine::GetInstance().GetNumConnectedClients() + 1));

			{
				auto localPlayer = std::make_unique<Player>();
				localPlayer->networkID = NetworkEngine::GetInstance().GenerateID();
				localPlayer->position = glm::vec3(0, 0, 0);
				localPlayer->scale = glm::vec3(1.5f, 1.5f, 1.5f);
				localPlayer->rotation = 0.f;
				localPlayer->meshType = Mesh::MESH_TYPE::QUAD;
				localPlayer->isLocal = true;
				localPlayer->color = { 0.2f, 1.f, 0.2f, 1.f };
				localPlayer->textured = true;
				localPlayer->textureType = Texture::TEXTURE_TYPE::TEX_PLAYER;

				Player* rawPlayerPtr = localPlayer.get();
				gameObjects.push_back(std::move(localPlayer));
				networkedObjects[rawPlayerPtr->networkID] = rawPlayerPtr;
				packet.push_back(static_cast<char>(EventType::ConnectedPlayer));
				NetworkID netNID = htonl(rawPlayerPtr->networkID);
				packet.insert(packet.end(), reinterpret_cast<char*>(&netNID),
					reinterpret_cast<char*>(&netNID) + sizeof(netNID));
			}

			for (int i = 0; i < NetworkEngine::GetInstance().GetNumConnectedClients(); ++i) {
				auto remotePlayer = std::make_unique<Player>();
				remotePlayer->networkID = NetworkEngine::GetInstance().GenerateID();
				remotePlayer->position = glm::vec3(0, 0, 0);
				remotePlayer->scale = glm::vec3(1.5f, 1.5f, 1.5f);
				remotePlayer->rotation = 0.f;
				remotePlayer->meshType = Mesh::MESH_TYPE::QUAD;
				remotePlayer->isLocal = false;
				remotePlayer->color = { 0.2f, 0.2f, 1.f, 1.f };
				remotePlayer->textured = true;
				remotePlayer->textureType = Texture::TEXTURE_TYPE::TEX_PLAYER;

				Player* rawRemote = remotePlayer.get();
				gameObjects.push_back(std::move(remotePlayer));
				networkedObjects[rawRemote->networkID] = rawRemote;
				packet.push_back(static_cast<char>(EventType::ConnectedPlayer));
				NetworkID netNID = htonl(rawRemote->networkID);
				packet.insert(packet.end(), reinterpret_cast<char*>(&netNID),
					reinterpret_cast<char*>(&netNID) + sizeof(netNID));
			}

			int i = 1;
			for (auto& client : NetworkEngine::GetInstance().clientManager.GetClients()) {
				std::vector<char> clientPacket(packet);
				clientPacket[i++ * 5 + 2] = static_cast<char>(EventType::SpawnPlayer);
				NetworkEngine::GetInstance().socketManager.SendToClient(client.address, clientPacket);
			}
			gameStarted = true;
			break;
		}
		case EventType::SpawnPlayer: {
			auto* spawnEvent = static_cast<SpawnPlayerEvent*>(event.get());
			auto newPlayer = std::make_unique<Player>();
			newPlayer->networkID = spawnEvent->networkID;
			newPlayer->position = glm::vec3(0, 0, 0);
			newPlayer->scale = glm::vec3(1.5f, 1.5f, 1.5f);
			newPlayer->rotation = 0.f;
			newPlayer->meshType = Mesh::MESH_TYPE::QUAD;
			newPlayer->isLocal = true;
			newPlayer->color = { 0.2f, 1.f, 0.2f, 1.f };
			newPlayer->textured = true;
			newPlayer->textureType = Texture::TEXTURE_TYPE::TEX_PLAYER;
			//std::cout << "Local Player Network ID: " << newPlayer->networkID << std::endl;

			Player* rawPlayerPtr = newPlayer.get();

			gameObjects.push_back(std::move(newPlayer));

			networkedObjects[rawPlayerPtr->networkID] = dynamic_cast<NetworkObject*>(rawPlayerPtr);
			break;
		}
		case EventType::ConnectedPlayer: {
			auto* joinEvent = static_cast<ConnectedPlayerEvent*>(event.get());
			auto newPlayer = std::make_unique<Player>();
			newPlayer->networkID = joinEvent->networkID;
			newPlayer->position = glm::vec3(0, 0, 0);
			newPlayer->scale = glm::vec3(1.5f, 1.5f, 1.5f);
			newPlayer->rotation = 0.f;
			newPlayer->meshType = Mesh::MESH_TYPE::QUAD;
			newPlayer->isLocal = false;
			newPlayer->color = { 0.2f, 0.2f, 1.f, 1.f };
			newPlayer->textured = true;
			newPlayer->textureType = Texture::TEXTURE_TYPE::TEX_PLAYER;

			Player* rawPlayerPtr = newPlayer.get();

			gameObjects.push_back(std::move(newPlayer));

			networkedObjects[rawPlayerPtr->networkID] = dynamic_cast<NetworkObject*>(rawPlayerPtr);
			break;
		}
		case EventType::PlayerUpdate: {
			auto* updateEvent = static_cast<PlayerUpdate*>(event.get());
			uint32_t rcvID;
			std::memcpy(&rcvID, &updateEvent->packet[1], sizeof(rcvID));
			rcvID = ntohl(rcvID);
			networkedObjects.at(rcvID)->Deserialize(updateEvent->packet.data());
			
			break;
		}
		case EventType::SpawnAsteroid: {
			auto* spawnEvent = static_cast<SpawnAsteroidEvent*>(event.get());
			auto asteroid = std::make_unique<Asteroid>();
			asteroid->networkID = spawnEvent->networkID;
			asteroid->position = spawnEvent->initialPosition;
			asteroid->scale = spawnEvent->initialScale;
			asteroid->velocity = spawnEvent->initialVelocity;
			asteroid->rotation = 0.f;
			asteroid->meshType = Mesh::MESH_TYPE::QUAD;
			asteroid->color = { 1.f, 1.f, 1.f, 1.f };
			asteroid->textured = true;
			asteroid->textureType = Texture::TEXTURE_TYPE::TEX_ASTEROID;

			Asteroid* rawAsteroid = asteroid.get();
			gameObjects.push_back(std::move(asteroid));
			networkedObjects[rawAsteroid->networkID] = rawAsteroid;
			std::cout << "Client asteroid spawned at: " << spawnEvent->initialPosition.x << ", " << spawnEvent->initialPosition.y << "\n";
			break;
		}
		default: break;
		}
	}
}

void AsteroidScene::Render() {
	GraphicsEngine::GetInstance().Render(gameObjects);
}

void AsteroidScene::Exit() {
	gameObjects.clear();
}
