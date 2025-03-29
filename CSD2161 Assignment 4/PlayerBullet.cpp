#include "PlayerBullet.hpp"

PlayerBullet::PlayerBullet(glm::vec3 pos, glm::vec3 dir, uint32_t ownerID)
	: dir(dir), speed(50.f), ownerID(ownerID)
{
    position = pos;
    rotation = static_cast<float>(atan2(dir.y, dir.x));
    meshType = Mesh::MESH_TYPE::QUAD;
    scale = glm::vec3(0.2f);
}

PlayerBullet::PlayerBullet(glm::vec3 pos, float rot, uint32_t ownerID)
    : speed(50.f), ownerID(ownerID)
{
	dir = glm::vec3(cos(rot), sin(rot), 0.f);
    position = pos;
    rotation = static_cast<float>(atan2(dir.y, dir.x));
    meshType = Mesh::MESH_TYPE::QUAD;
    scale = glm::vec3(0.2f);
}

void PlayerBullet::Update(double dt) {
	position += dir * speed * static_cast<float>(dt);
}

std::vector<char> PlayerBullet::Serialize()
{
    std::vector<char> ret;
    return ret;
}

void PlayerBullet::Deserialize(const char*)
{
}
