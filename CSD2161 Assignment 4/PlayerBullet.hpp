#pragma once

#include "GameObject.hpp"
#include "Networking/NetworkObject.hpp"
#include <glm/vec3.hpp>

class PlayerBullet : public GameObject, public NetworkObject {
public:
	glm::vec3 dir;
	float speed;
	uint32_t ownerID;

	PlayerBullet(glm::vec3 pos, glm::vec3 dir, uint32_t ownerID);
	PlayerBullet(glm::vec3 pos, float rot, uint32_t ownerID);


	void Update(double) override;

	std::vector<char> Serialize() override;
	void Deserialize(const char*) override;
};

