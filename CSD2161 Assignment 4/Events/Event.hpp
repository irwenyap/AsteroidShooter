#pragma once
#include <memory>
#include <string>
#include <glm/vec3.hpp>
#include <WinSock2.h>
#include "../Networking/NetworkObject.hpp"
#include "../Networking/NetworkUtils.hpp" // Include for serialization utils

using NetworkID = uint32_t;
using EventID = uint32_t;

enum class EventType {
    //Control
    RequestStartGame, //Client ask host to start the game
    StartGame, // Host starts the game (confirmation)

    //State
    PlayerJoined, //Info about player joining (broadcast by host) 
    PlayerLeft, //Info about player leaving (broadcast by host)
    SpawnPlayer, //Host spawns a player
    SpawnAsteroid, //Host spawns an asteroid

    //Actions
    FireBullet, //Player fires a bullet
	Collision, //Collision between two objects

    PlayerUpdate, //Player position update

    //Rendering
    RenderBullet, //Render bullet
    RenderAsteroid, //Render asteroid
};

struct GameEvent {
    EventType type;
    NetworkID id;
    virtual ~GameEvent() = default;
};

struct FireBulletEvent : public GameEvent {
    glm::vec3 position;
    float rotation;
    uint32_t ownerId;

    FireBulletEvent(const glm::vec3& pos, float rot, uint32_t owner)
        : position(pos), rotation(rot), ownerId(owner)
    {
        type = EventType::FireBullet;
    }

    // Simple serialization for FireBulletEvent
    std::vector<char> Serialize() const {
        std::vector<char> data;
        // Type is handled by the wrapper message (GAME_EVENT / BROADCAST_EVENT)
        NetworkUtils::WriteVec3(data, position);
        NetworkUtils::WriteToPacket(data, NetworkUtils::FloatToNetwork(rotation), NetworkUtils::DATA_TYPE::DT_LONG);
        uint32_t netOwnerId = htonl(ownerId);
        NetworkUtils::WriteToPacket(data, netOwnerId, NetworkUtils::DATA_TYPE::DT_LONG);
        return data;
    }
};

struct StartGameEvent : public GameEvent {
    StartGameEvent() {
        type = EventType::StartGame;
    }
};

struct RequestStartGameEvent : public GameEvent {
    RequestStartGameEvent() {
        type = EventType::RequestStartGame;
    }
};


struct SpawnPlayerEvent : public GameEvent {
    uint32_t networkID;

    SpawnPlayerEvent(uint32_t id)
        : networkID(id)
    {
        type = EventType::SpawnPlayer;
    }
};

struct PlayerJoinedEvent : public GameEvent {
    uint32_t networkID;
    glm::vec3 initialPosition;
    float initialRotation;

    PlayerJoinedEvent(uint32_t id) : networkID(id)
    {
        type = EventType::PlayerJoined;
    }
};

struct PlayerLeftEvent : public GameEvent {
	uint32_t networkID;

	PlayerLeftEvent(uint32_t id) : networkID(id)
	{
		type = EventType::PlayerLeft;
	}
};

struct SpawnAsteroidEvent : public GameEvent {
    uint32_t networkID;
    glm::vec3 initialPosition;
    glm::vec3 initialScale;
    glm::vec3 initialVelocity;

    SpawnAsteroidEvent(uint32_t id, const std::vector<char>& packet) : networkID(id) {
        //int16_t posX, posY, scaX, scaY, velX, velY;

        NetworkUtils::ReadVec3(packet.data(), 5, initialPosition);
        NetworkUtils::ReadVec3(packet.data(), 17, initialScale);
        NetworkUtils::ReadVec3(packet.data(), 29, initialVelocity);

        type = EventType::SpawnAsteroid;
    }
};

struct CollisionEvent : public GameEvent {
    CollisionEvent(NetworkID a, NetworkID b)
        : idA(a), idB(b) {
        type = EventType::Collision;
    }
    NetworkID idA;
    NetworkID idB;

    // Simple serialization for FireBulletEvent
    std::vector<char> Serialize() const {
        std::vector<char> data;
        
        uint32_t netA = htonl(idA);
        NetworkUtils::WriteToPacket(data, netA, NetworkUtils::DATA_TYPE::DT_LONG);
        uint32_t netB = htonl(idB);
        NetworkUtils::WriteToPacket(data, netB, NetworkUtils::DATA_TYPE::DT_LONG);
        return data;
    }
};

struct PlayerUpdate : public GameEvent {
    std::vector<char> packet;

    PlayerUpdate(const char* rawPacket, size_t length) {
        type = EventType::PlayerUpdate;
        packet.assign(rawPacket, rawPacket + length);
    }
};
