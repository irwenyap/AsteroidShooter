#include "EventQueue.hpp"

EventQueue& EventQueue::GetInstance() {
    static EventQueue eq;
    return eq;
}

void EventQueue::Push(std::unique_ptr<GameEvent> event) {
    events.emplace_back(std::move(event));
}

std::vector<std::unique_ptr<GameEvent>> EventQueue::Drain() {
    std::vector<std::unique_ptr<GameEvent>> drained = std::move(events);
    events.clear();
    return drained;
}