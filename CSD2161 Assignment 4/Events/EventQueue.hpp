#pragma once

#include <vector>
#include <memory>
#include <glm/vec3.hpp>
#include "Event.hpp"

class EventQueue {
public:
    static EventQueue& GetInstance();

    void Push(std::unique_ptr<GameEvent> event);

    std::vector<std::unique_ptr<GameEvent>> Drain();

private:
    std::vector<std::unique_ptr<GameEvent>> events;
	std::vector<std::unique_ptr<GameEvent>> waitingEvents;
};