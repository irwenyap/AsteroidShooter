#pragma once

#include "GameObject.hpp"
#include "Networking/NetworkObject.hpp"
#include <glm/vec3.hpp>

class PlayerBullet : public GameObject, public NetworkObject {
public:
	uint32_t playerID = 0;
	glm::vec3 dir;
	float speed;

	PlayerBullet(glm::vec3 pos, glm::vec3 dir, uint32_t ownerID);

	void Update(double) override;
	void FixedUpdate(double) override;

	std::vector<char> Serialize() override;
	void Deserialize(const char*) override;
};

