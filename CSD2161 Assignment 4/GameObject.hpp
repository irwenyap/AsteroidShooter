#pragma once
#include <cstdint>
#include "Graphics/Mesh.hpp"
#include "Graphics//Texture.hpp"
#include <glm/vec3.hpp>
#include <glm/vec4.hpp>

class GameObject {
public:
	enum GO_TYPE {
		GO_PLAYER,
		GO_BULLET,
		GO_ASTEROID
	};

	//uint32_t id;
	bool isActive = true;
	GO_TYPE type;
	glm::vec4 color = glm::vec4(1.0f);
	glm::vec3 position{ 0.f, 0.f, 0.f };
	glm::vec3 scale{ 1.f, 1.f, 1.f };
	float rotation{ 0.f };

	Mesh::MESH_TYPE meshType;
	Texture::TEXTURE_TYPE textureType;
	bool textured = false;

	virtual void Update(double) = 0;
	virtual void FixedUpdate(double) = 0;

	virtual ~GameObject() = default;
};

