#pragma once

#include <vector>
#include <memory>
#include <unordered_map>
#include "GameObject.hpp"
#include "Networking/NetworkObject.hpp"

class AsteroidScene {
public:
	void Initialize();
	void Update(double);
	void FixedUpdate(double);
	void ProcessEvents();
	void Render();
	void Exit();
	
private:
	std::vector<std::unique_ptr<GameObject>> gameObjects;
	std::unordered_map<NetworkID, NetworkObject*> networkedObjects;
};

