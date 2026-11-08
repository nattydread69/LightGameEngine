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

#include "LightGameEngine/Input/InputState.h"

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace lge
{

// Index of a named gameplay action. Look it up once at setup time; queries by id
// avoid string comparisons in the per-tick path.
struct ActionId
{
	std::uint32_t value = UINT32_MAX;

	[[nodiscard]] constexpr bool isValid() const noexcept
	{
		return value != UINT32_MAX;
	}
	friend constexpr bool operator==(ActionId, ActionId) noexcept = default;
};

struct InputBinding
{
	enum class Device : std::uint8_t
	{
		Keyboard,
		Mouse
	};

	Device device = Device::Keyboard;
	Key key = Key::Unknown;
	MouseButton button = MouseButton::Left;

	[[nodiscard]] static constexpr InputBinding fromKey(Key const key) noexcept
	{
		return InputBinding {Device::Keyboard, key, MouseButton::Left};
	}

	[[nodiscard]] static constexpr InputBinding fromMouse(MouseButton const button) noexcept
	{
		return InputBinding {Device::Mouse, Key::Unknown, button};
	}

	friend constexpr bool operator==(InputBinding const&, InputBinding const&) noexcept = default;
};

// Maps named gameplay actions ("MoveForward", "Fire") to any number of raw inputs.
// Gameplay asks about actions, never about keys, so controls can be rebound without
// touching game logic.
class ActionMap
{
public:
	// Returns the existing id if the action is already defined.
	ActionId define(std::string_view name);
	[[nodiscard]] std::optional<ActionId> find(std::string_view name) const;
	[[nodiscard]] std::string_view name(ActionId action) const;
	[[nodiscard]] std::size_t actionCount() const noexcept
	{
		return actions_.size();
	}

	// Duplicate bindings are ignored.
	void bind(ActionId action, InputBinding binding);
	ActionId bind(std::string_view name, Key key);
	ActionId bind(std::string_view name, MouseButton button);
	void clearBindings(ActionId action);
	[[nodiscard]] std::vector<InputBinding> const& bindings(ActionId action) const;

	// True while any binding is held.
	[[nodiscard]] bool isActive(ActionId action, InputState const& input) const;
	// True in the tick in which any binding was pressed.
	[[nodiscard]] bool wasTriggered(ActionId action, InputState const& input) const;
	// True in the tick in which any binding was released.
	[[nodiscard]] bool wasReleased(ActionId action, InputState const& input) const;
	// -1, 0 or +1 from a pair of opposing actions; both held cancel out.
	[[nodiscard]] double axis(ActionId negative, ActionId positive, InputState const& input) const;

private:
	struct Action
	{
		std::string name;
		std::vector<InputBinding> bindings;
	};

	Action const& action(ActionId id) const;

	std::vector<Action> actions_;
};

}
