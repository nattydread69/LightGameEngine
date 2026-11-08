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
