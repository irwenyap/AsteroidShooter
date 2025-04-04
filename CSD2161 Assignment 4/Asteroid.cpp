#include "Asteroid.hpp"
#include <glm/glm.hpp>
#include "Networking/NetworkUtils.hpp"
#include "Networking/NetworkEngine.hpp"

Asteroid::Asteroid()
{
}

void Asteroid::Update(double)
{
	
}

void Asteroid::FixedUpdate(double fixedDT)
{
	position += velocity * static_cast<float>(fixedDT);
}

std::vector<char> Asteroid::Serialize()
{
	std::vector<char> packet;

	NetworkUtils::WriteToPacket(packet, networkID, NetworkUtils::DATA_TYPE::DT_LONG);
	NetworkUtils::WriteToPacket(packet, NetworkEngine::GetInstance().localTick, NetworkUtils::DATA_TYPE::DT_LONG);
	NetworkUtils::WriteVec2(packet, position);
	NetworkUtils::WriteToPacket(packet, NetworkUtils::FloatToNetwork(rotation), NetworkUtils::DATA_TYPE::DT_LONG);
	NetworkUtils::WriteVec2(packet, velocity);

	return packet;
}

void Asteroid::Deserialize(const char* packet)
{
	uint32_t packetTick{};
	NetworkUtils::ReadFromPacket(packet, 5, packetTick, NetworkUtils::DATA_TYPE::DT_LONG);
	if (packetTick < lastReceivedTick) return;

	lastReceivedTick = packetTick;
	NetworkUtils::ReadVec2(packet, 9, position);
	uint32_t tempRot{};
	NetworkUtils::ReadFromPacket(packet, 17, tempRot, NetworkUtils::DATA_TYPE::DT_LONG);
	rotation = NetworkUtils::NetworkToFloat(tempRot);
	NetworkUtils::ReadVec2(packet, 21, velocity);
}
