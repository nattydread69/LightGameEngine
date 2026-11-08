#include "LightGameEngine/Core/GameClock.h"

#include <algorithm>
#include <stdexcept>

namespace lge
{

GameClock::GameClock(double const fpsWindowSeconds)
	: fpsWindowSeconds_(fpsWindowSeconds)
{
	if (!(fpsWindowSeconds_ > 0.0))
	{
		throw std::invalid_argument("GameClock: FPS window must be positive");
	}
}

double GameClock::beginFrame(double const nowSeconds)
{
	if (!startSeconds_)
	{
		startSeconds_ = nowSeconds;
		previousSeconds_ = nowSeconds;
		windowStartSeconds_ = nowSeconds;
	}

	// A non-monotonic clock source must not produce negative frame times.
	double const delta = std::max(nowSeconds - previousSeconds_, 0.0);
	previousSeconds_ = std::max(nowSeconds, previousSeconds_);

	timing_.frameSeconds = delta;
	timing_.totalSeconds = previousSeconds_ - *startSeconds_;
	++timing_.frameCount;

	if (timing_.frameCount == 1)
	{
		// The first frame opens the measurement window; FPS counts frame intervals.
		return delta;
	}
	++windowFrames_;
	double const windowElapsed = previousSeconds_ - windowStartSeconds_;
	if (windowElapsed >= fpsWindowSeconds_)
	{
		timing_.framesPerSecond = static_cast<double>(windowFrames_) / windowElapsed;
		windowFrames_ = 0;
		windowStartSeconds_ = previousSeconds_;
	}
	return delta;
}

void GameClock::recordSimulationStep(double const stepSeconds)
{
	++timing_.tickCount;
	timing_.simulationSeconds = static_cast<double>(timing_.tickCount) * stepSeconds;
}

void GameClock::reset()
{
	startSeconds_.reset();
	previousSeconds_ = 0.0;
	windowStartSeconds_ = 0.0;
	windowFrames_ = 0;
	timing_ = {};
}

}
