#pragma once

#include <vector>

using Tick = uint32_t;
using NetworkID = uint32_t;

class NetworkObject {
public:
	virtual std::vector<char> Serialize() = 0;
	virtual void Deserialize(const char*) = 0;
	bool isLocal = false;
	Tick lastReceivedTick = 0;
	NetworkID networkID = 0; // to be assigned by host/server
};

