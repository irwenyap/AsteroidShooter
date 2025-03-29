#pragma once
#include <memory>
#include <string>
#include <glm/vec3.hpp>
#include <WinSock2.h>
#include "../Networking/NetworkObject.hpp"
using NetworkID = uint32_t;


enum class EventType {
    StartGame,
    SpawnPlayer,
    ConnectedPlayer,
    FireBullet,
    SpawnAsteroid,
    PlayerUpdate,
    Collision
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

struct ConnectedPlayerEvent : public GameEvent {
    uint32_t networkID;
    glm::vec3 initialPosition;
    float initialRotation;

    ConnectedPlayerEvent(uint32_t id) : networkID(id)
    {
        type = EventType::ConnectedPlayer;
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

struct CollisionEvent : public GameEvent {
    CollisionEvent(NetworkID a, NetworkID b)
        : idA(a), idB(b) {
        type = EventType::Collision;
    }
    NetworkID idA;
    NetworkID idB;
};

struct PlayerUpdate : public GameEvent {
    std::vector<char> packet;

    PlayerUpdate(const char* rawPacket, size_t length) {
        type = EventType::PlayerUpdate;
        packet.assign(rawPacket, rawPacket + length);
    }
};
