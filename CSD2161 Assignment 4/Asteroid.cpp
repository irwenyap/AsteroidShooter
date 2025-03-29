#include "Asteroid.hpp"
#include <glm/glm.hpp>

Asteroid::Asteroid()
{
}

void Asteroid::Update(double dt)
{
	position += velocity * static_cast<float>(dt);
}

std::vector<char> Asteroid::Serialize()
{
	return std::vector<char>();
}

void Asteroid::Deserialize(const char*)
{
}
