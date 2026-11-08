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

#include <glm/glm.hpp>

#include <bitset>

namespace lge
{

// Raw device state with edge detection that is consumed per simulation tick.
//
// The platform layer reports state changes whenever it polls (once per rendered
// frame). Presses and releases are accumulated as *pending* edges. At the start of
// each simulation tick the engine calls beginTick(), which publishes the pending
// edges to that tick and clears them. Consequences:
//   * A press is seen by exactly one tick: it is not repeated when one rendered
//     frame runs several ticks, and it is not lost when a frame runs no ticks
//     (it stays pending until the next tick).
//   * A press and release that both happen between two ticks still reports
//     wasPressed() and wasReleased() in that tick, even though isDown() is false.
//   * isDown() always reflects the most recently polled level.
class InputState
{
public:
	// Platform-facing: report the current level of a key or button. Repeated reports
	// of an unchanged level are ignored, so polling every frame is safe.
	void setKeyDown(Key key, bool down) noexcept;
	void setMouseButtonDown(MouseButton button, bool down) noexcept;
	void setMousePosition(glm::dvec2 const& positionPixels) noexcept;
	// Platform-facing: accumulate wheel movement (positive = away from the user).
	void addMouseScroll(double steps) noexcept;

	// Engine-facing: publish pending edges to the next tick.
	void beginTick() noexcept;
	// Drops all state, e.g. when the window loses focus.
	void reset() noexcept;

	// Gameplay-facing queries.
	[[nodiscard]] bool isDown(Key key) const noexcept;
	[[nodiscard]] bool wasPressed(Key key) const noexcept;
	[[nodiscard]] bool wasReleased(Key key) const noexcept;
	[[nodiscard]] bool isDown(MouseButton button) const noexcept;
	[[nodiscard]] bool wasPressed(MouseButton button) const noexcept;
	[[nodiscard]] bool wasReleased(MouseButton button) const noexcept;
	[[nodiscard]] glm::dvec2 mousePosition() const noexcept
	{
		return mousePosition_;
	}
	// Wheel movement accumulated since the last call, which clears it. Unlike key
	// edges it is not tied to beginTick(), so frame-rate camera zoom can use it.
	[[nodiscard]] double takeMouseScroll() noexcept;
	// Mouse movement accumulated since the previous tick, in pixels.
	[[nodiscard]] glm::dvec2 mouseDelta() const noexcept
	{
		return tickMouseDelta_;
	}

private:
	std::bitset<keyCount> keysDown_;
	std::bitset<keyCount> pendingKeyPresses_;
	std::bitset<keyCount> pendingKeyReleases_;
	std::bitset<keyCount> tickKeyPresses_;
	std::bitset<keyCount> tickKeyReleases_;

	std::bitset<mouseButtonCount> buttonsDown_;
	std::bitset<mouseButtonCount> pendingButtonPresses_;
	std::bitset<mouseButtonCount> pendingButtonReleases_;
	std::bitset<mouseButtonCount> tickButtonPresses_;
	std::bitset<mouseButtonCount> tickButtonReleases_;

	glm::dvec2 mousePosition_ {0.0};
	glm::dvec2 pendingMouseDelta_ {0.0};
	glm::dvec2 tickMouseDelta_ {0.0};
	double pendingScroll_ = 0.0;
	bool hasMousePosition_ = false;
};

}
