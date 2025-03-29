#include "Timer.hpp"

void Timer::Start() {
	begin = previous = Clock::now();
}

void Timer::Update() {
    current = Clock::now();
    Duration elapsed = current - begin;
    Duration delta = current - previous;

    previous = current;

    elapsedTime = DurationInSeconds(elapsed);
    deltaTime = DurationInSeconds(delta);

    ++frame_counter;
    fpsTimer += deltaTime;

    if (fpsTimer >= 1.0) {
        fps = frame_counter;
        frame_counter = 0;
        fpsTimer -= 1.0;
    }

    CalculateNumOfSteps(deltaTime);
}

void Timer::CalculateNumOfSteps(double dt) {
    fixedAccumulator += dt;
    const int maxSteps = 10;
    fixedSteps = 0;
    while (fixedAccumulator >= fixedDeltaTime && fixedSteps < maxSteps) {
        fixedAccumulator -= fixedDeltaTime;
        fixedSteps++;
    }
}
