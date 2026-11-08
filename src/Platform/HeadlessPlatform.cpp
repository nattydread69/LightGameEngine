// SPDX-License-Identifier: LGPL-3.0-or-later
//
// Light Game Engine
// Copyright (C) 2026 and onwards Dr. Nathanael John Inkson
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published
// by the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see <https://www.gnu.org/licenses/>.

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
