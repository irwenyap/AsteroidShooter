#pragma once

#include <vector>
#include <memory>
#include <string>
#include <unordered_map>
#include <glm/mat4x4.hpp>
#include "Mesh.hpp"
#include "Texture.hpp"

typedef unsigned int GLuint;
class GameObject;

class GraphicsEngine {
	GLuint shaderProgram;
	glm::mat4 view;
	glm::mat4 projection;

	std::unordered_map<Mesh::MESH_TYPE, Mesh> meshes;
	std::unordered_map<Texture::TEXTURE_TYPE, GLuint> textures;
public:
	static GraphicsEngine& GetInstance();

	void Init();
	void Render(std::vector<std::unique_ptr<GameObject>>&);
	void UpdateProjection(int width, int height);
	GLuint LoadTexture(const std::string& filePath);
};

