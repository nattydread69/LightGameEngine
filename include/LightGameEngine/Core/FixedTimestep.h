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

namespace lge
{

struct FixedTimestepConfig
{
	// Simulation step length. The default is 60 Hz.
	double stepSeconds = 1.0 / 60.0;
	// Real frame durations longer than this are clamped, so a debugger pause or a
	// window drag does not ask the simulation to catch up on seconds of time.
	double maxFrameSeconds = 0.25;
	// Upper bound on steps executed in one frame. Time that would need more steps
	// is discarded rather than accumulated, preventing a "spiral of death" where a
	// slow simulation falls further behind every frame.
	std::uint32_t maxStepsPerFrame = 8;
};

struct FixedStepResult
{
	// Number of simulation steps the caller must execute this frame.
	std::uint32_t steps = 0;
	// Fraction of a step left in the accumulator, in [0, 1). Used to interpolate
	// between the previous and current simulation states when rendering.
	double alpha = 0.0;
	// Real time that was thrown away by the frame clamp or the step limit.
	double droppedSeconds = 0.0;
};

// Converts variable real frame durations into a whole number of fixed simulation
// steps using an accumulator. It does not run the steps itself, so the caller
// stays in control of what a step does.
class FixedTimestep
{
public:
	explicit FixedTimestep(FixedTimestepConfig const& config = {});

	FixedStepResult advance(double frameSeconds);
	void reset();

	[[nodiscard]] FixedTimestepConfig const& config() const noexcept
	{
		return config_;
	}
	[[nodiscard]] double stepSeconds() const noexcept
	{
		return config_.stepSeconds;
	}
	[[nodiscard]] double accumulatedSeconds() const noexcept
	{
		return accumulator_;
	}
	[[nodiscard]] double alpha() const noexcept;

private:
	FixedTimestepConfig config_;
	double accumulator_ = 0.0;
};

}
