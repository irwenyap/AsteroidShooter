﻿#pragma once

#include "GameObject.hpp"
#include "Networking/NetworkObject.hpp"
#include <glm/trigonometric.hpp>

class Player : public GameObject, public NetworkObject {
	glm::vec3 prevPos;
	float prevRot;
	glm::vec3 targetPos;
	float targetRot;
	glm::vec3 velocity = glm::vec3(0.f);
	float rotationSpeed = glm::radians(180.f);
	float thrust = 10.f;
	float drag = 0.98f;
public:
	Player();
	void Update(double) override;

	std::vector<char> Serialize() override;
	void Deserialize(const char*) override;
};

