#pragma once
#include <memory>
#include <string>
#include <glm/vec3.hpp>
#include <WinSock2.h>

enum class EventType : uint8_t{
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
	AsteroidHit, //Asteroid is hit by a bullet
    
	PlayerUpdate, //Player position update

	//Rendering
	RenderBullet, //Render bullet
	RenderAsteroid, //Render asteroid
};

struct GameEvent {
    EventType type;
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
};

struct StartGameEvent : public GameEvent {
    StartGameEvent() {
        type = EventType::StartGame;
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
    float initialRotation{};

    PlayerJoinedEvent(uint32_t id) : networkID(id)
    {
        type = EventType::PlayerJoined;
    }
};

struct SpawnAsteroidEvent : public GameEvent {
    uint32_t networkID;
    glm::vec3 initialPosition;
    glm::vec3 initialScale;
    glm::vec3 initialVelocity;

    SpawnAsteroidEvent(uint32_t id, std::vector<char>& packet) : networkID(id) {
        int16_t posX, posY, scaX, scaY, velX, velY;

        std::memcpy(&posX, &packet[7], sizeof(posX));
        std::memcpy(&posY, &packet[9], sizeof(posY));
        std::memcpy(&scaX, &packet[11], sizeof(scaX));
        std::memcpy(&scaY, &packet[13], sizeof(scaY));
        std::memcpy(&velX, &packet[15], sizeof(velX));
        std::memcpy(&velY, &packet[17], sizeof(velY));

        initialPosition = { static_cast<int16_t>(ntohs(posX)) / 100.f, static_cast<int16_t>(ntohs(posY)) / 100.f, 0.f };
        initialScale = { static_cast<int16_t>(ntohs(scaX)) / 100.f, static_cast<int16_t>(ntohs(scaY)) / 100.f, 1.f };
        initialVelocity = { static_cast<int16_t>(ntohs(velX)) / 100.f, static_cast<int16_t>(ntohs(velY)) / 100.f, 0.f };

        type = EventType::SpawnAsteroid;
    }
};

struct PlayerUpdate : public GameEvent {
    std::vector<char> packet;

    PlayerUpdate(const char* rawPacket, size_t length) {
        type = EventType::PlayerUpdate;
        packet.assign(rawPacket, rawPacket + length);
    }
};
