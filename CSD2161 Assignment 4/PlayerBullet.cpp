#include "PlayerBullet.hpp"

PlayerBullet::PlayerBullet(glm::vec3 pos, glm::vec3 dir, uint32_t ownerID)
    : dir(dir), speed(50.f), playerID(ownerID)
{
    position = pos;
    rotation = static_cast<float>(atan2(dir.y, dir.x));
    meshType = Mesh::MESH_TYPE::QUAD;
    scale = glm::vec3(0.2f);
}

void PlayerBullet::Update(double dt) {
	position += dir * speed * static_cast<float>(dt);
}

void PlayerBullet::FixedUpdate(double)
{
}

std::vector<char> PlayerBullet::Serialize()
{
    std::vector<char> ret;
    return ret;
}

void PlayerBullet::Deserialize(const char*)
{
}
