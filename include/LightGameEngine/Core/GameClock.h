#pragma once

#include <cstdint>
#include <optional>

namespace lge
{

struct FrameTiming
{
	// Real duration of the most recent frame (unclamped).
	double frameSeconds = 0.0;
	// Real time since the first frame.
	double totalSeconds = 0.0;
	// Simulated time: the number of executed steps multiplied by the step length.
	double simulationSeconds = 0.0;
	std::uint64_t frameCount = 0;
	std::uint64_t tickCount = 0;
	// Frames per second averaged over the last completed measurement window.
	double framesPerSecond = 0.0;
};

// Records real and simulated time. Time is supplied by the caller (from the platform
// layer) so the clock is deterministic under test.
class GameClock
{
public:
	explicit GameClock(double fpsWindowSeconds = 1.0);

	// Starts a frame at the given monotonic time and returns the real time elapsed
	// since the previous frame (zero for the first frame).
	double beginFrame(double nowSeconds);
	void recordSimulationStep(double stepSeconds);
	void reset();

	[[nodiscard]] FrameTiming const& timing() const noexcept
	{
		return timing_;
	}

private:
	double fpsWindowSeconds_;
	std::optional<double> startSeconds_;
	double previousSeconds_ = 0.0;
	double windowStartSeconds_ = 0.0;
	std::uint64_t windowFrames_ = 0;
	FrameTiming timing_;
};

}
