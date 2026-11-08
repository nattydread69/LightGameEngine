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

#include <cstddef>
#include <cstdint>
#include <string_view>

namespace lge
{

// Engine-defined key identifiers. Platform layers translate their native key codes
// into these, so gameplay code never sees GLFW or OS key codes.
enum class Key : std::uint16_t
{
	Unknown = 0,
	A, B, C, D, E, F, G, H, I, J, K, L, M, N, O, P, Q, R, S, T, U, V, W, X, Y, Z,
	Num0, Num1, Num2, Num3, Num4, Num5, Num6, Num7, Num8, Num9,
	Space, Enter, Escape, Tab, Backspace, Insert, Delete,
	Left, Right, Up, Down, PageUp, PageDown, Home, End,
	F1, F2, F3, F4, F5, F6, F7, F8, F9, F10, F11, F12,
	LeftShift, RightShift, LeftControl, RightControl, LeftAlt, RightAlt,
	Minus, Equal, Comma, Period, Slash, Semicolon, Apostrophe, LeftBracket, RightBracket, Backslash, GraveAccent,
	Count
};

enum class MouseButton : std::uint8_t
{
	Left = 0,
	Right,
	Middle,
	Count
};

inline constexpr std::size_t keyCount = static_cast<std::size_t>(Key::Count);
inline constexpr std::size_t mouseButtonCount = static_cast<std::size_t>(MouseButton::Count);

[[nodiscard]] std::string_view keyName(Key key) noexcept;

}
