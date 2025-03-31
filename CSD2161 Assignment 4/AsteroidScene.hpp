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

	std::unordered_map<NetworkID, NetworkObject*>& GetNetworkedObjects();
	NetworkObject* GetNetworkedObject(NetworkID id);
	void AddGameObject(std::unique_ptr<GameObject> obj, NetworkObject* netObj); // To add objects received over network
	void RemoveGameObject(NetworkID id); // To remove objects (e.g., on PlayerLeft)

	void AddScore(NetworkID playerId, int points);
	int GetScore(NetworkID playerId) const;
	const std::unordered_map<NetworkID, int>& GetAllScores() const;
	std::unordered_map<NetworkID, int>& GetPlayerScores();

private:
	std::vector<std::unique_ptr<GameObject>> gameObjects;
	std::unordered_map<NetworkID, NetworkObject*> networkedObjects;
	std::unordered_map<NetworkID, int> playerScores;
};

