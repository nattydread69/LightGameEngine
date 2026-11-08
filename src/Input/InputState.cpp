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

#include <array>

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

void InputState::beginTick() noexcept
{
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
	*this = InputState {};
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
