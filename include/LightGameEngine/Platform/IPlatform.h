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
#include <functional>

namespace lge
{

class InputState;

struct WindowExtent
{
	std::uint32_t width = 0;
	std::uint32_t height = 0;

	friend constexpr bool operator==(WindowExtent, WindowExtent) noexcept = default;
};

// The host the engine runs inside: it drives the frame loop, owns the window (if
// any) and supplies time and input. Implementations: HeadlessPlatform (no window)
// and the LightVulkanGraphics backend (window owned by the graphics library).
//
// The loop is driven by the platform rather than the engine because some hosts own
// their event loop; the engine supplies a per-frame callback instead.
class IPlatform
{
public:
	// Runs one frame; returns false to stop the loop.
	using FrameFunction = std::function<bool()>;

	IPlatform() = default;
	IPlatform(IPlatform const&) = delete;
	IPlatform& operator=(IPlatform const&) = delete;
	virtual ~IPlatform() = default;

	// Calls `frame` repeatedly until it returns false or the user closes the window.
	virtual void run(FrameFunction const& frame) = 0;
	// Monotonic time in seconds.
	[[nodiscard]] virtual double timeSeconds() const = 0;
	// Reports the current device state into `input`. Called once per frame.
	virtual void pollInput(InputState& input) = 0;
	// Drawable size in pixels; zero when minimised or headless.
	[[nodiscard]] virtual WindowExtent framebufferExtent() const = 0;
	// Ends run() after the current frame.
	virtual void requestClose() = 0;
};

}
