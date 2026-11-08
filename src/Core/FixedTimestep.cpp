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
