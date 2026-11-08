#include "LightGameEngine/Core/FixedTimestep.h"

#include <algorithm>
#include <cmath>
#include <stdexcept>

namespace lge
{

FixedTimestep::FixedTimestep(FixedTimestepConfig const& config)
	: config_(config)
{
	if (!(config_.stepSeconds > 0.0) || !std::isfinite(config_.stepSeconds))
	{
		throw std::invalid_argument("FixedTimestep: stepSeconds must be positive and finite");
	}
	if (!(config_.maxFrameSeconds > 0.0))
	{
		throw std::invalid_argument("FixedTimestep: maxFrameSeconds must be positive");
	}
	if (config_.maxStepsPerFrame == 0)
	{
		throw std::invalid_argument("FixedTimestep: maxStepsPerFrame must be at least 1");
	}
}

FixedStepResult FixedTimestep::advance(double const frameSeconds)
{
	FixedStepResult result;
	double elapsed = std::isfinite(frameSeconds) ? std::max(frameSeconds, 0.0) : 0.0;
	if (elapsed > config_.maxFrameSeconds)
	{
		result.droppedSeconds += elapsed - config_.maxFrameSeconds;
		elapsed = config_.maxFrameSeconds;
	}
	accumulator_ += elapsed;

	// A tiny tolerance absorbs floating-point drift, so that e.g. six frames of
	// exactly 1/60 s at 60 Hz yield six steps rather than five.
	double const epsilon = config_.stepSeconds * 1e-9;
	while (accumulator_ + epsilon >= config_.stepSeconds && result.steps < config_.maxStepsPerFrame)
	{
		accumulator_ -= config_.stepSeconds;
		++result.steps;
	}
	accumulator_ = std::max(accumulator_, 0.0);

	if (accumulator_ >= config_.stepSeconds)
	{
		// The step limit was hit. Keep only the fractional remainder so the next frame
		// does not inherit a backlog it can never clear.
		double const kept = std::fmod(accumulator_, config_.stepSeconds);
		result.droppedSeconds += accumulator_ - kept;
		accumulator_ = kept;
	}

	result.alpha = alpha();
	return result;
}

void FixedTimestep::reset()
{
	accumulator_ = 0.0;
}

double FixedTimestep::alpha() const noexcept
{
	return std::clamp(accumulator_ / config_.stepSeconds, 0.0, 1.0);
}

}
