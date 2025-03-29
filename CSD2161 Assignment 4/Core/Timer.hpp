#ifndef TIMER_HPP
#define TIMER_HPP

#include <chrono>

/**
 * \class Timer
 * \brief A high-resolution timer class for tracking elapsed time and frame updates.
 *
 * The Timer class provides functionality for tracking time elapsed since the timer started and the time
 * between frames (delta time). It also calculates frames per second (FPS).
 */
class Timer {
    using Clock = std::chrono::high_resolution_clock;   /**< Alias for the high-resolution clock. */
    using TimePoint = std::chrono::time_point<Clock>;   /**< Alias for time points in the high-resolution clock. */
    using Duration = std::chrono::duration<double>;     /**< Alias for durations measured in double-precision seconds. */

    TimePoint begin;            /**< The time point when the timer was started. */
    TimePoint previous;         /**< The time point from the previous update. */
    TimePoint current;          /**< The current time point from the last update. */
    double elapsedTime = 0.0;   /**< The total elapsed time since the timer started. */
    double deltaTime = 0.0;     /**< The time between the last two updates (frame time). */

    double fixedDeltaTime = 1.0 / 60.0;         /**< The fixed delta time used to update time-based systems. */
    double fixedAccumulator = 0.0;              /**< Used in computing the difference between deltaTime and fixedDeltaTime. */
    int fixedSteps = 0;
    int frame_counter = 0;
    double fpsTimer = 0.0;
    int fps = 0;

public:
    /**
     * \brief Default constructor for the Timer class.
     */
    Timer() = default;

    /**
     * \brief Starts the timer by capturing the current time as the start time.
     */
    void Start();

    /**
     * \brief Updates the timer by calculating delta time and elapsed time.
     *
     * This method should be called once per frame to keep track of frame timing and FPS.
     */
    void Update();

    /**
     * \brief Retrieves the time point when the timer started.
     * \return The start time point.
     */
    inline TimePoint GetBegin() const { return begin; }

    /**
     * \brief Retrieves the current time point of the timer.
     * \return The current time point.
     */
    inline TimePoint GetCurrent() const { return current; }

    /**
     * \brief Retrieves the time between the last two updates (frame time).
     * \return The delta time in seconds.
     */
    inline double GetDeltaTime() const { return deltaTime; }

    /**
     * \brief Retrieves the fixed delta time.
     * \return The fixed delta time in seconds.
     */
    inline double GetFixedDT() const { return fixedDeltaTime; }

    /**
     * \brief Retrieves the total elapsed time since the timer started.
     * \return The elapsed time in seconds.
     */
    inline double GetElapsedTime() const { return elapsedTime; }

    /**
     * \brief Retrieves the current frames per second (FPS).
     * \return The FPS.
     */
    inline int GetFPS() { return fps; }

    /**
     * \brief Retrieves the number of steps to update time-based systems.
     * \return The number of steps to update time-based systems.
     */
    inline int GetFixedSteps() const { return fixedSteps; }

    inline void SetFixedDeltaTime(double dt) { fixedDeltaTime = dt; }

private:
    /**
     * \brief Calculates the number of steps to update time-based systems.
     * \param dt The delta time between the current and previous frame.
     */
    void CalculateNumOfSteps(double dt);

    /**
     * \brief Converts a duration into seconds as a double.
     * \param duration The duration to convert.
     * \return The duration in seconds.
     */
    static double DurationInSeconds(const Duration& duration) {
        return duration.count();
    }
};

#endif