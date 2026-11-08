#include "LightGameEngine/Platform/HeadlessPlatform.h"

#include "LightGameEngine/Input/InputState.h"

#include <stdexcept>

namespace lge
{

HeadlessPlatform::HeadlessPlatform()
	: HeadlessPlatform(Config {})
{
}

HeadlessPlatform::HeadlessPlatform(Config const& config)
	: config_(config)
{
	if (!(config_.frameSeconds >= 0.0))
	{
		throw std::invalid_argument("HeadlessPlatform: frameSeconds must not be negative");
	}
}

void HeadlessPlatform::scheduleKey(std::uint64_t const frame, Key const key, bool const down)
{
	scheduled_.push_back(ScheduledKey {frame, key, down});
}

void HeadlessPlatform::run(FrameFunction const& frame)
{
	closeRequested_ = false;
	while (!closeRequested_ && (config_.maxFrames == 0 || frame_ < config_.maxFrames))
	{
		bool const keepRunning = frame();
		++frame_;
		timeSeconds_ += config_.frameSeconds;
		if (!keepRunning)
		{
			break;
		}
	}
}

void HeadlessPlatform::pollInput(InputState& input)
{
	for (ScheduledKey const& event : scheduled_)
	{
		if (event.frame == frame_)
		{
			input.setKeyDown(event.key, event.down);
		}
	}
}

}
