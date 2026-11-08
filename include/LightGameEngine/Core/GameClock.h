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
