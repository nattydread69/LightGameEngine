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

#pragma once

#include "LightGameEngine/Input/InputTypes.h"
#include "LightGameEngine/Platform/IPlatform.h"

#include <cstdint>
#include <vector>

namespace lge
{

// Windowless platform for tests, servers and CI. Time is virtual: every frame
// advances it by a fixed amount, so runs are fully deterministic and independent of
// machine speed. Input can be scripted per frame.
class HeadlessPlatform final : public IPlatform
{
public:
	struct Config
	{
		// Virtual time added per frame. Deliberately need not match the simulation step.
		double frameSeconds = 1.0 / 60.0;
		// Stop after this many frames; 0 runs until the frame callback returns false.
		std::uint64_t maxFrames = 0;
		WindowExtent extent {1280, 720};
	};

	HeadlessPlatform();
	explicit HeadlessPlatform(Config const& config);

	// Schedules a key level change, applied when the given frame polls input
	// (frames count from 0).
	void scheduleKey(std::uint64_t frame, Key key, bool down);

	void run(FrameFunction const& frame) override;
	[[nodiscard]] double timeSeconds() const override
	{
		return timeSeconds_;
	}
	void pollInput(InputState& input) override;
	[[nodiscard]] WindowExtent framebufferExtent() const override
	{
		return config_.extent;
	}
	void requestClose() override
	{
		closeRequested_ = true;
	}

	void setExtent(WindowExtent extent) noexcept
	{
		config_.extent = extent;
	}
	[[nodiscard]] std::uint64_t framesRun() const noexcept
	{
		return frame_;
	}

private:
	struct ScheduledKey
	{
		std::uint64_t frame;
		Key key;
		bool down;
	};

	Config config_;
	std::vector<ScheduledKey> scheduled_;
	std::uint64_t frame_ = 0;
	double timeSeconds_ = 0.0;
	bool closeRequested_ = false;
};

}
