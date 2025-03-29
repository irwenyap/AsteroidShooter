#include "Player.hpp"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include "InputManager.hpp"
#include "Events/EventQueue.hpp"
#include "Networking/NetworkEngine.hpp"
#include "Networking/NetworkUtils.hpp"
#include <iostream>
#include <glm/geometric.hpp>
#include <glm/gtc/constants.hpp>


// utility function to be moved
float LerpAngle(float a, float b, float t) {
    float delta = fmodf(b - a + glm::pi<float>(), glm::two_pi<float>()) - glm::pi<float>();
    return a + delta * t;
}

Player::Player() {
    prevPos = position;
    prevRot = rotation;
    targetPos = position;
}

void Player::Update(double dt)
{
    if (isLocal) {
        InputManager& input = InputManager::GetInstance();

        // Rotate left/right
        if (input.GetKey(GLFW_KEY_A)) {
            rotation += rotationSpeed * static_cast<float>(dt);
        }
        if (input.GetKey(GLFW_KEY_D)) {
            rotation -= rotationSpeed * static_cast<float>(dt);
        }

        if (input.GetKey(GLFW_KEY_W)) {
            glm::vec2 forward = glm::vec2(cos(rotation), sin(rotation));
            velocity += glm::vec3(forward * thrust * static_cast<float>(dt), 0.0f);
        }

        velocity *= drag;

        position += velocity * static_cast<float>(dt);

        if (input.GetKeyDown(GLFW_KEY_SPACE)) {
			if (NetworkEngine::GetInstance().isHosting) {
				EventQueue::GetInstance().Push(std::make_unique<FireBulletEvent>(position, rotation, 0));
            }
            else {
				EventQueue::GetInstance().Push(std::make_unique<ReqFireBulletEvent>(position, rotation, id, networkID));
            }
        }

        if (glm::length(position - prevPos) > 1.f) {
            if (NetworkEngine::GetInstance().isClient) {
                NetworkEngine::GetInstance().socketManager.SendToHost(Serialize());
                prevPos = position;
            } else {
                NetworkEngine::GetInstance().SendToAllClients(Serialize());
                prevPos = position;
            }
            //std::cout << "Positional Data Sent\n";
        }
    } else {
        float interpSpeed = 5.0f; // higher = faster interpolation
        position = glm::mix(position, targetPos, interpSpeed * static_cast<float>(dt));

        if (std::abs(rotation - targetRot) > 0.01f)
            rotation = LerpAngle(rotation, targetRot, interpSpeed * static_cast<float>(dt));

        //std::cout << position.x << " :PACKET: " << position.x << std::endl;
    }
}

std::vector<char> Player::Serialize() {
    std::vector<char> packet;
    packet.push_back(NetworkEngine::CMDID::GAME_DATA);

    NetworkID netID = htonl(networkID);
    packet.insert(packet.end(), reinterpret_cast<char*>(&netID), reinterpret_cast<char*>(&netID) + sizeof(netID));

    int16_t x = static_cast<int16_t>(position.x * 100);
    int16_t y = static_cast<int16_t>(position.y * 100);
    int16_t rot = static_cast<int16_t>(rotation * 10);
    x = htons(x);
    y = htons(y);
    rot = htons(rot);
    packet.insert(packet.end(), reinterpret_cast<char*>(&x), reinterpret_cast<char*>(&x) + sizeof(x));
    packet.insert(packet.end(), reinterpret_cast<char*>(&y), reinterpret_cast<char*>(&y) + sizeof(y));
    packet.insert(packet.end(), reinterpret_cast<char*>(&rot), reinterpret_cast<char*>(&rot) + sizeof(rot));

    // NOT WORKING YET FOR NOW IDK Y YETTTT USE THE TOP ONE FOR NOW

    //NetworkUtils::WriteVec3(packet, position);
    //NetworkUtils::WriteToPacket(packet, NetworkUtils::FloatToNetwork(rotation), NetworkUtils::DATA_TYPE::DT_LONG);

    return packet;
}

void Player::Deserialize(const char* packet) {
    //float test1 = static_cast<float>(static_cast<int8_t>(packet[1]));
    //float test2 = static_cast<float>(static_cast<int8_t>(packet[2]));
    //std::cout << test1 << " :PACKET: " << test2 << std::endl;
    int16_t x;
    int16_t y;
    int16_t rot;
    std::memcpy(&x, &packet[5], sizeof(x));
    std::memcpy(&y, &packet[7], sizeof(y));
    std::memcpy(&rot, &packet[9], sizeof(rot));
    x = static_cast<int16_t>(ntohs(x));
    y = static_cast<int16_t>(ntohs(y));
    rot = static_cast<int16_t>(ntohs(rot));
    targetPos.x = x / 100.f;
    targetPos.y = y / 100.f;
    targetRot = rot / 10.f;

    // NOT WORKING YET FOR NOW IDK Y YETTTT USE THE TOP ONE FOR NOW

    //NetworkUtils::ReadVec3(packet, 5, position);
    //uint32_t tempRot{};
    //NetworkUtils::ReadFromPacket(packet, 17, tempRot, NetworkUtils::DATA_TYPE::DT_LONG);
    //rotation = NetworkUtils::NetworkToFloat(tempRot);
}
