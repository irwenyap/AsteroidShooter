#include "Player.hpp"

#include <glm/vec2.hpp>
#include <glm/vec3.hpp>
#include "InputManager.hpp"
#include "Networking/NetworkEngine.hpp"
#include "Events/Event.hpp"
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

void Player::Update(double)
{
    if (isLocal) {
        InputManager& input = InputManager::GetInstance();

        if (input.GetKeyDown(GLFW_KEY_SPACE)) {
            // Don't push locally, send to server for lockstep

            auto fireEvent = std::make_unique<FireBulletEvent>(position, rotation, networkID);
			if (NetworkEngine::GetInstance().isClient) {
				NetworkEngine::GetInstance().SendEventToServer(std::move(fireEvent));
			}
			else if (NetworkEngine::GetInstance().isHosting) {
 				NetworkEngine::GetInstance().ServerBroadcastEvent(std::move(fireEvent));
			}
            
            // EventQueue::GetInstance().Push(std::make_unique<FireBulletEvent>(position, rotation, networkID)); // OLD WAY
        }

        if (glm::length(position - prevPos) > 1.f || std::abs(rotation - prevRot) > 0.1f) {
            if (NetworkEngine::GetInstance().isClient) {
                NetworkEngine::GetInstance().socketManager.SendToHost(Serialize());
                prevPos = position;
                prevRot = rotation;
            } else {
                NetworkEngine::GetInstance().SendToAllClients(Serialize());
                prevPos = position;
                prevRot = rotation;
            }
        }
    }
}

void Player::FixedUpdate(double fixedDt) {
    if (isLocal) {
        InputManager& input = InputManager::GetInstance();

        // Rotate left/right
        if (input.GetKey(GLFW_KEY_A)) {
            rotation += rotationSpeed * static_cast<float>(fixedDt);
        }
        if (input.GetKey(GLFW_KEY_D)) {
            rotation -= rotationSpeed * static_cast<float>(fixedDt);
        }

        if (input.GetKey(GLFW_KEY_W)) {
            glm::vec2 forward = glm::vec2(cos(rotation), sin(rotation));
            velocity += glm::vec3(forward * thrust * static_cast<float>(fixedDt), 0.0f);
        }

        velocity *= drag;

        position += velocity * static_cast<float>(fixedDt);
    } else {
        float interpolationSpeed = 2.0f;

        uint32_t currentTick = NetworkEngine::GetInstance().localTick;
        int tickDelta = static_cast<int>(currentTick) - static_cast<int>(lastReceivedTick);
        tickDelta = glm::clamp(tickDelta, 0, 15); // clamp to prevent over-extrapolation

        float extrapolationTime = static_cast<float>(tickDelta) / 60.0f;

        // predict where the player should be based on last known velocity
        glm::vec3 extrapolatedTarget = targetPos + lastReceivedVelocity * extrapolationTime;

        // smoothly move towards extrapolated target
        position = glm::mix(position, extrapolatedTarget, interpolationSpeed * static_cast<float>(fixedDt));

        if (std::abs(rotation - targetRot) > 0.01f)
            rotation = LerpAngle(rotation, targetRot, interpolationSpeed * static_cast<float>(fixedDt));
    }
}

std::vector<char> Player::Serialize() {
    std::vector<char> packet;
    packet.push_back(NetworkEngine::CMDID::GAME_DATA);

    NetworkUtils::WriteToPacket(packet, networkID, NetworkUtils::DATA_TYPE::DT_LONG);
    NetworkUtils::WriteToPacket(packet, NetworkEngine::GetInstance().localTick, NetworkUtils::DATA_TYPE::DT_LONG);
    NetworkUtils::WriteVec2(packet, position);
    NetworkUtils::WriteToPacket(packet, NetworkUtils::FloatToNetwork(rotation), NetworkUtils::DATA_TYPE::DT_LONG);
    NetworkUtils::WriteVec2(packet, velocity);

    return packet;
}

void Player::Deserialize(const char* packet) {
    uint32_t packetTick{};
    NetworkUtils::ReadFromPacket(packet, 5, packetTick, NetworkUtils::DATA_TYPE::DT_LONG);
    if (packetTick < lastReceivedTick) return;

    lastReceivedTick = packetTick;
    NetworkUtils::ReadVec2(packet, 9, targetPos);
    uint32_t tempRot{};
    NetworkUtils::ReadFromPacket(packet, 17, tempRot, NetworkUtils::DATA_TYPE::DT_LONG);
    targetRot = NetworkUtils::NetworkToFloat(tempRot);
    NetworkUtils::ReadVec2(packet, 21, lastReceivedVelocity);
}
