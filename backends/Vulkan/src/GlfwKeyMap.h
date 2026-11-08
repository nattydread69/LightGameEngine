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

// Private to the Vulkan backend: translation from GLFW key codes to engine keys.

#include "LightGameEngine/Input/InputTypes.h"

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <array>

namespace lge
{

struct KeyMapping
{
	Key key;
	int glfwKey;
};

inline std::array<KeyMapping, keyCount - 1> const& glfwKeyMap()
{
	static std::array<KeyMapping, keyCount - 1> const map {{
		{Key::A, GLFW_KEY_A}, {Key::B, GLFW_KEY_B}, {Key::C, GLFW_KEY_C}, {Key::D, GLFW_KEY_D},
		{Key::E, GLFW_KEY_E}, {Key::F, GLFW_KEY_F}, {Key::G, GLFW_KEY_G}, {Key::H, GLFW_KEY_H},
		{Key::I, GLFW_KEY_I}, {Key::J, GLFW_KEY_J}, {Key::K, GLFW_KEY_K}, {Key::L, GLFW_KEY_L},
		{Key::M, GLFW_KEY_M}, {Key::N, GLFW_KEY_N}, {Key::O, GLFW_KEY_O}, {Key::P, GLFW_KEY_P},
		{Key::Q, GLFW_KEY_Q}, {Key::R, GLFW_KEY_R}, {Key::S, GLFW_KEY_S}, {Key::T, GLFW_KEY_T},
		{Key::U, GLFW_KEY_U}, {Key::V, GLFW_KEY_V}, {Key::W, GLFW_KEY_W}, {Key::X, GLFW_KEY_X},
		{Key::Y, GLFW_KEY_Y}, {Key::Z, GLFW_KEY_Z},
		{Key::Num0, GLFW_KEY_0}, {Key::Num1, GLFW_KEY_1}, {Key::Num2, GLFW_KEY_2}, {Key::Num3, GLFW_KEY_3},
		{Key::Num4, GLFW_KEY_4}, {Key::Num5, GLFW_KEY_5}, {Key::Num6, GLFW_KEY_6}, {Key::Num7, GLFW_KEY_7},
		{Key::Num8, GLFW_KEY_8}, {Key::Num9, GLFW_KEY_9},
		{Key::Space, GLFW_KEY_SPACE}, {Key::Enter, GLFW_KEY_ENTER}, {Key::Escape, GLFW_KEY_ESCAPE},
		{Key::Tab, GLFW_KEY_TAB}, {Key::Backspace, GLFW_KEY_BACKSPACE}, {Key::Insert, GLFW_KEY_INSERT},
		{Key::Delete, GLFW_KEY_DELETE},
		{Key::Left, GLFW_KEY_LEFT}, {Key::Right, GLFW_KEY_RIGHT}, {Key::Up, GLFW_KEY_UP}, {Key::Down, GLFW_KEY_DOWN},
		{Key::PageUp, GLFW_KEY_PAGE_UP}, {Key::PageDown, GLFW_KEY_PAGE_DOWN}, {Key::Home, GLFW_KEY_HOME},
		{Key::End, GLFW_KEY_END},
		{Key::F1, GLFW_KEY_F1}, {Key::F2, GLFW_KEY_F2}, {Key::F3, GLFW_KEY_F3}, {Key::F4, GLFW_KEY_F4},
		{Key::F5, GLFW_KEY_F5}, {Key::F6, GLFW_KEY_F6}, {Key::F7, GLFW_KEY_F7}, {Key::F8, GLFW_KEY_F8},
		{Key::F9, GLFW_KEY_F9}, {Key::F10, GLFW_KEY_F10}, {Key::F11, GLFW_KEY_F11}, {Key::F12, GLFW_KEY_F12},
		{Key::LeftShift, GLFW_KEY_LEFT_SHIFT}, {Key::RightShift, GLFW_KEY_RIGHT_SHIFT},
		{Key::LeftControl, GLFW_KEY_LEFT_CONTROL}, {Key::RightControl, GLFW_KEY_RIGHT_CONTROL},
		{Key::LeftAlt, GLFW_KEY_LEFT_ALT}, {Key::RightAlt, GLFW_KEY_RIGHT_ALT},
		{Key::Minus, GLFW_KEY_MINUS}, {Key::Equal, GLFW_KEY_EQUAL}, {Key::Comma, GLFW_KEY_COMMA},
		{Key::Period, GLFW_KEY_PERIOD}, {Key::Slash, GLFW_KEY_SLASH}, {Key::Semicolon, GLFW_KEY_SEMICOLON},
		{Key::Apostrophe, GLFW_KEY_APOSTROPHE}, {Key::LeftBracket, GLFW_KEY_LEFT_BRACKET},
		{Key::RightBracket, GLFW_KEY_RIGHT_BRACKET}, {Key::Backslash, GLFW_KEY_BACKSLASH},
		{Key::GraveAccent, GLFW_KEY_GRAVE_ACCENT},
	}};
	return map;
}

}
