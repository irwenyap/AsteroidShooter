#pragma once
#include "GameObject.hpp"
#include "Networking/NetworkObject.hpp"

class Asteroid : public GameObject, public NetworkObject {
public:
	Asteroid();
	void Update(double) override;

	std::vector<char> Serialize() override;
	void Deserialize(const char*) override;

	glm::vec3 velocity = glm::vec3(0.f);


};

