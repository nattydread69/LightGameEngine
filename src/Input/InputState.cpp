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

#include "LightGameEngine/Input/InputState.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace lge
{

namespace
{

template <typename Enum>
constexpr std::size_t indexOf(Enum const value) noexcept
{
	return static_cast<std::size_t>(value);
}

template <std::size_t N>
void applyLevel(std::bitset<N>& down, std::bitset<N>& pressed, std::bitset<N>& released,
	std::size_t const index, bool const isDown) noexcept
{
	if (index >= N || down.test(index) == isDown)
	{
		return;
	}
	down.set(index, isDown);
	if (isDown)
	{
		pressed.set(index);
	}
	else
	{
		released.set(index);
	}
}

}

void InputState::setKeyDown(Key const key, bool const down) noexcept
{
	if (key == Key::Unknown)
	{
		return;
	}
	applyLevel(keysDown_, pendingKeyPresses_, pendingKeyReleases_, indexOf(key), down);
}

void InputState::setMouseButtonDown(MouseButton const button, bool const down) noexcept
{
	applyLevel(buttonsDown_, pendingButtonPresses_, pendingButtonReleases_, indexOf(button), down);
}

void InputState::setMousePosition(glm::dvec2 const& positionPixels) noexcept
{
	// The first report establishes the position without producing a jump in delta.
	if (hasMousePosition_)
	{
		pendingMouseDelta_ += positionPixels - mousePosition_;
	}
	mousePosition_ = positionPixels;
	hasMousePosition_ = true;
}

void InputState::addMouseScroll(double const steps) noexcept
{
	pendingScroll_ += steps;
}

double InputState::takeMouseScroll() noexcept
{
	double const steps = pendingScroll_;
	pendingScroll_ = 0.0;
	return steps;
}

void InputState::setGamepadInput(Gamepad& gamepad, std::size_t const index, bool const down) noexcept
{
	applyLevel(gamepad.down, gamepad.pendingPresses, gamepad.pendingReleases, index, down);
}

double InputState::deadzoned(double const value) const noexcept
{
	double const magnitude = std::abs(value);
	if (magnitude <= gamepadDeadzone_)
	{
		return 0.0;
	}
	return std::copysign((magnitude - gamepadDeadzone_) / (1.0 - gamepadDeadzone_), value);
}

void InputState::setGamepadDeadzone(double const deadzone) noexcept
{
	// Below 1 so the rescale never divides by zero.
	gamepadDeadzone_ = std::clamp(deadzone, 0.0, 0.95);
}

void InputState::setGamepadConnected(std::size_t const pad, bool const connected) noexcept
{
	if (pad >= maxGamepads)
	{
		return;
	}
	Gamepad& gamepad = gamepads_[pad];
	if (!connected)
	{
		for (std::size_t i = 0; i < gamepadInputCount; ++i)
		{
			setGamepadInput(gamepad, i, false);
		}
		gamepad.axes.fill(0.0);
	}
	gamepad.connected = connected;
}

void InputState::setGamepadButtonDown(std::size_t const pad, GamepadButton const button, bool const down) noexcept
{
	if (pad >= maxGamepads || indexOf(button) >= gamepadButtonCount)
	{
		return;
	}
	setGamepadInput(gamepads_[pad], indexOf(button), down);
}

void InputState::setGamepadAxis(std::size_t const pad, GamepadAxis const axis, double const value) noexcept
{
	if (pad >= maxGamepads || indexOf(axis) >= gamepadAxisCount)
	{
		return;
	}
	Gamepad& gamepad = gamepads_[pad];
	// NaN from a faulty driver is treated as centred.
	double const clamped = std::isnan(value) ? 0.0 : std::clamp(value, -1.0, 1.0);
	gamepad.axes[indexOf(axis)] = clamped;
	double const effective = deadzoned(clamped);
	std::size_t const base = gamepadButtonCount + indexOf(axis) * 2;
	setGamepadInput(gamepad, base + indexOf(AxisDirection::Negative), effective <= -0.5);
	setGamepadInput(gamepad, base + indexOf(AxisDirection::Positive), effective >= 0.5);
}

void InputState::beginTick() noexcept
{
	for (Gamepad& gamepad : gamepads_)
	{
		gamepad.tickPresses = gamepad.pendingPresses;
		gamepad.tickReleases = gamepad.pendingReleases;
		gamepad.pendingPresses.reset();
		gamepad.pendingReleases.reset();
	}
	tickKeyPresses_ = pendingKeyPresses_;
	tickKeyReleases_ = pendingKeyReleases_;
	tickButtonPresses_ = pendingButtonPresses_;
	tickButtonReleases_ = pendingButtonReleases_;
	tickMouseDelta_ = pendingMouseDelta_;

	pendingKeyPresses_.reset();
	pendingKeyReleases_.reset();
	pendingButtonPresses_.reset();
	pendingButtonReleases_.reset();
	pendingMouseDelta_ = glm::dvec2 {0.0};
}

void InputState::reset() noexcept
{
	double const deadzone = gamepadDeadzone_;
	*this = InputState {};
	gamepadDeadzone_ = deadzone;
}

bool InputState::isDown(Key const key) const noexcept
{
	return indexOf(key) < keyCount && keysDown_.test(indexOf(key));
}

bool InputState::wasPressed(Key const key) const noexcept
{
	return indexOf(key) < keyCount && tickKeyPresses_.test(indexOf(key));
}

bool InputState::wasReleased(Key const key) const noexcept
{
	return indexOf(key) < keyCount && tickKeyReleases_.test(indexOf(key));
}

bool InputState::isDown(MouseButton const button) const noexcept
{
	return indexOf(button) < mouseButtonCount && buttonsDown_.test(indexOf(button));
}

bool InputState::wasPressed(MouseButton const button) const noexcept
{
	return indexOf(button) < mouseButtonCount && tickButtonPresses_.test(indexOf(button));
}

bool InputState::wasReleased(MouseButton const button) const noexcept
{
	return indexOf(button) < mouseButtonCount && tickButtonReleases_.test(indexOf(button));
}

bool InputState::isGamepadConnected(std::size_t const pad) const noexcept
{
	return pad < maxGamepads && gamepads_[pad].connected;
}

bool InputState::isDown(GamepadButton const button, std::size_t const pad) const noexcept
{
	return pad < maxGamepads && indexOf(button) < gamepadButtonCount && gamepads_[pad].down.test(indexOf(button));
}

bool InputState::wasPressed(GamepadButton const button, std::size_t const pad) const noexcept
{
	return pad < maxGamepads && indexOf(button) < gamepadButtonCount && gamepads_[pad].tickPresses.test(indexOf(button));
}

bool InputState::wasReleased(GamepadButton const button, std::size_t const pad) const noexcept
{
	return pad < maxGamepads && indexOf(button) < gamepadButtonCount && gamepads_[pad].tickReleases.test(indexOf(button));
}

double InputState::gamepadAxis(GamepadAxis const axis, std::size_t const pad) const noexcept
{
	if (pad >= maxGamepads || indexOf(axis) >= gamepadAxisCount)
	{
		return 0.0;
	}
	return deadzoned(gamepads_[pad].axes[indexOf(axis)]);
}

glm::dvec2 InputState::gamepadStick(GamepadStick const stick, std::size_t const pad) const noexcept
{
	if (pad >= maxGamepads)
	{
		return glm::dvec2 {0.0};
	}
	std::size_t const first = stick == GamepadStick::Left ? indexOf(GamepadAxis::LeftX) : indexOf(GamepadAxis::RightX);
	glm::dvec2 const raw {gamepads_[pad].axes[first], gamepads_[pad].axes[first + 1]};
	double const length = glm::length(raw);
	if (length <= gamepadDeadzone_)
	{
		return glm::dvec2 {0.0};
	}
	double const scaled = std::min(1.0, (length - gamepadDeadzone_) / (1.0 - gamepadDeadzone_));
	return raw / length * scaled;
}

namespace
{

std::size_t axisInputIndex(GamepadAxis const axis, AxisDirection const direction) noexcept
{
	return gamepadButtonCount + indexOf(axis) * 2 + indexOf(direction);
}

}

bool InputState::isDown(GamepadAxis const axis, AxisDirection const direction, std::size_t const pad) const noexcept
{
	return pad < maxGamepads && indexOf(axis) < gamepadAxisCount && gamepads_[pad].down.test(axisInputIndex(axis, direction));
}

bool InputState::wasPressed(GamepadAxis const axis, AxisDirection const direction, std::size_t const pad) const noexcept
{
	return pad < maxGamepads && indexOf(axis) < gamepadAxisCount && gamepads_[pad].tickPresses.test(axisInputIndex(axis, direction));
}

bool InputState::wasReleased(GamepadAxis const axis, AxisDirection const direction, std::size_t const pad) const noexcept
{
	return pad < maxGamepads && indexOf(axis) < gamepadAxisCount && gamepads_[pad].tickReleases.test(axisInputIndex(axis, direction));
}

std::string_view keyName(Key const key) noexcept
{
	static constexpr std::array<std::string_view, keyCount> names {
		"Unknown",
		"A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z",
		"0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
		"Space", "Enter", "Escape", "Tab", "Backspace", "Insert", "Delete",
		"Left", "Right", "Up", "Down", "PageUp", "PageDown", "Home", "End",
		"F1", "F2", "F3", "F4", "F5", "F6", "F7", "F8", "F9", "F10", "F11", "F12",
		"LeftShift", "RightShift", "LeftControl", "RightControl", "LeftAlt", "RightAlt",
		"Minus", "Equal", "Comma", "Period", "Slash", "Semicolon", "Apostrophe", "LeftBracket", "RightBracket", "Backslash", "GraveAccent"};
	std::size_t const index = indexOf(key);
	return index < names.size() ? names[index] : std::string_view {"Unknown"};
}

}
