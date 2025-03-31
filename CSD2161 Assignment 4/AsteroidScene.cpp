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
#include <unordered_set>

#define MAX_LOCAL_GAMEOBJECTS 1250
const double asteroidSpawnRate = 5.0;
double asteroidSpawnTimer = 0.0;
bool gameStarted = false;

AsteroidScene* g_AsteroidScene = nullptr;


void AsteroidScene::Initialize() {
	GraphicsEngine::GetInstance().Init();
	gameObjects.reserve(MAX_LOCAL_GAMEOBJECTS);

	g_AsteroidScene = this; // Set the global pointer
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

	// Setting only host to detect for collision
	if (!NetworkEngine::GetInstance().isHosting)
		return; 
	
	//Loop through all pairs of game objects to check for collisions
	for (size_t i = 0; i < gameObjects.size(); ++i) {
		for (size_t j = i + 1; j < gameObjects.size(); ++j) {

			auto* a = dynamic_cast<NetworkObject*>(gameObjects[i].get());
			auto* b = dynamic_cast<NetworkObject*>(gameObjects[j].get());
			if (!a || !b) continue;

			//Determining object if its bullet/asteroid
			auto* bullet = dynamic_cast<PlayerBullet*>(a);
			auto* asteroid = dynamic_cast<Asteroid*>(b);

			//swap if undetected
			if (!bullet || !asteroid) {
				bullet = dynamic_cast<PlayerBullet*>(b);
				asteroid = dynamic_cast<Asteroid*>(a);
			}

			//if not bullet-asteroid pair continue
			if (!bullet || !asteroid) continue;

			//distance between bullet and asteroid
			float dist = glm::length(bullet->position - asteroid->position);

			//CollisionDistance 
			float collisionDist = (bullet->scale.x + asteroid->scale.x) * 0.5f;

			//Push a collisionEvent into event queue of both objects
			if (dist < collisionDist) {
				EventQueue::GetInstance().Push(std::make_unique<CollisionEvent>(bullet->networkID, asteroid->networkID));
			}
		}
	}

}


void AsteroidScene::ProcessEvents() {
	for (auto& event : EventQueue::GetInstance().Drain()) {
		switch (event->type) {
		case EventType::FireBullet: {
			auto* fire = static_cast<FireBulletEvent*>(event.get());
			glm::vec3 dir(cos(fire->rotation), sin(fire->rotation), 0.f);
			auto bullet = std::make_unique<PlayerBullet>(fire->position, dir);
			bullet->networkID = event.get()->id;
			NetworkObject* rawBullet = bullet.get();
			gameObjects.push_back(std::move(bullet));
			networkedObjects[rawBullet->networkID] = rawBullet;

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
				packet.push_back(static_cast<char>(EventType::PlayerJoined));
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
				packet.push_back(static_cast<char>(EventType::PlayerJoined));
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
		case EventType::PlayerJoined: {
			auto* joinEvent = static_cast<PlayerJoinedEvent*>(event.get());
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
		case EventType::Collision: {
			auto* collision = static_cast<CollisionEvent*>(event.get());
			NetworkID idA = collision->idA;
			NetworkID idB = collision->idB;

			//std::cout << "[CollisionEvent] Received. Objects to delete: ID A = " << idA << ", ID B = " << idB << "\n";

			//set to store which network ID to delete
			std::unordered_set<NetworkID> idsToDelete;

			//Check btwn idA and idB
			NetworkID ids[2] = { idA, idB };
			for (int i = 0; i < 2; ++i) {
				NetworkID id = ids[i];
				auto it = networkedObjects.find(id);
				
				if (it != networkedObjects.end()) {
					auto* obj = it->second;
					if (auto* player = dynamic_cast<Player*>(obj)) {
						if (player->isLocal) {
							//std::cout << "[SKIP] Tried to delete local player with ID " << id << " — skipping\n";
							continue;
						}
					}
					//std::cout << "[Delete] Deleting object with ID " << id << ", type: " << typeid(*obj).name() << "\n";
					idsToDelete.insert(id);
				}
			}

			//Remove obj from main gameObjects list
			auto it = std::remove_if(gameObjects.begin(), gameObjects.end(),
				[&](const std::unique_ptr<GameObject>& go) {
					auto* netObj = dynamic_cast<NetworkObject*>(go.get());
					return netObj && idsToDelete.count(netObj->networkID);
				});
			gameObjects.erase(it, gameObjects.end());

			//remove from networkObjects map
			for (NetworkID id : idsToDelete) {
				networkedObjects.erase(id);
			}

			//Host sends the collision even to all clients so it delets the same obj
			if (NetworkEngine::GetInstance().isHosting) {
				std::vector<char> packet;
				packet.push_back(NetworkEngine::CMDID::GAME_EVENT);
				packet.push_back(1); // one event
				packet.push_back(static_cast<char>(EventType::Collision));

				NetworkID netIDA = htonl(idA);
				NetworkID netIDB = htonl(idB);
				packet.insert(packet.end(), reinterpret_cast<char*>(&netIDA), reinterpret_cast<char*>(&netIDA) + sizeof(netIDA));
				packet.insert(packet.end(), reinterpret_cast<char*>(&netIDB), reinterpret_cast<char*>(&netIDB) + sizeof(netIDB));

				//std::cout << "[Host] Sending CollisionEvent to clients (IDs: " << idA << ", " << idB << ")\n";
				//Send the packet to the clients
				NetworkEngine::GetInstance().SendToAllClients(packet);
			}

			break;
		}
		case EventType::PlayerLeft: {
			auto* playerLeft = static_cast<PlayerLeftEvent*>(event.get());
			std::cout << "[Scene] Processing PlayerLeftEvent for NetworkID: " << playerLeft->networkID << std::endl;
			RemoveGameObject(playerLeft->networkID);
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

	g_AsteroidScene = nullptr; // Clear the global pointer
}

std::unordered_map<NetworkID, NetworkObject*>& AsteroidScene::GetNetworkedObjects() {
	return networkedObjects;
	
}

NetworkObject * AsteroidScene::GetNetworkedObject(NetworkID id) {
	auto it = networkedObjects.find(id);
	if (it != networkedObjects.end()) {
		return it->second;
		
	}
	return nullptr;
	
}

void AsteroidScene::AddGameObject(std::unique_ptr<GameObject> obj, NetworkObject * netObj) {
	if (netObj) {
		// Check if ID already exists due to race conditions
		if (networkedObjects.count(netObj->networkID)) {
		std::cerr << "[Scene] Warning: NetworkObject with ID " << netObj->networkID << " already exists. Replacing." << std::endl;
			     // Consider removing the old one first if necessary
				RemoveGameObject(netObj->networkID);
			
		}
		networkedObjects[netObj->networkID] = netObj;
		std::cout << "[Scene] Added NetworkObject with ID: " << netObj->networkID << std::endl;
		
	}
	 gameObjects.push_back(std::move(obj));
}

void AsteroidScene::RemoveGameObject(NetworkID id) {
	std::cout << "[Scene] Attempting to remove GameObject with NetworkID: " << id << std::endl;
	networkedObjects.erase(id);
	
		// Remove from the main gameObjects vector
		gameObjects.erase(std::remove_if(gameObjects.begin(), gameObjects.end(),
			[id](const std::unique_ptr<GameObject>& obj) {
				// Check if the GameObject is also a NetworkObject and has the matching ID
				NetworkObject * netObj = dynamic_cast<NetworkObject*>(obj.get());
				return netObj && netObj->networkID == id;
				}), gameObjects.end());
	
}

