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

#include "LightGameEngine/Input/ActionMap.h"

#include <algorithm>
#include <stdexcept>

namespace lge
{

namespace
{

// Gamepad bindings match whichever pad satisfies them, so any connected pad works.
template <typename Query>
bool anyGamepad(Query&& query)
{
	for (std::size_t pad = 0; pad < maxGamepads; ++pad)
	{
		if (query(pad))
		{
			return true;
		}
	}
	return false;
}

bool bindingDown(InputBinding const& binding, InputState const& input)
{
	switch (binding.device)
	{
	case InputBinding::Device::Keyboard:
		return input.isDown(binding.key);
	case InputBinding::Device::Mouse:
		return input.isDown(binding.button);
	case InputBinding::Device::GamepadButton:
		return anyGamepad([&](std::size_t pad) { return input.isDown(binding.gamepadButton, pad); });
	case InputBinding::Device::GamepadAxis:
		return anyGamepad([&](std::size_t pad) { return input.isDown(binding.gamepadAxis, binding.direction, pad); });
	}
	return false;
}

bool bindingPressed(InputBinding const& binding, InputState const& input)
{
	switch (binding.device)
	{
	case InputBinding::Device::Keyboard:
		return input.wasPressed(binding.key);
	case InputBinding::Device::Mouse:
		return input.wasPressed(binding.button);
	case InputBinding::Device::GamepadButton:
		return anyGamepad([&](std::size_t pad) { return input.wasPressed(binding.gamepadButton, pad); });
	case InputBinding::Device::GamepadAxis:
		return anyGamepad([&](std::size_t pad) { return input.wasPressed(binding.gamepadAxis, binding.direction, pad); });
	}
	return false;
}

bool bindingReleased(InputBinding const& binding, InputState const& input)
{
	switch (binding.device)
	{
	case InputBinding::Device::Keyboard:
		return input.wasReleased(binding.key);
	case InputBinding::Device::Mouse:
		return input.wasReleased(binding.button);
	case InputBinding::Device::GamepadButton:
		return anyGamepad([&](std::size_t pad) { return input.wasReleased(binding.gamepadButton, pad); });
	case InputBinding::Device::GamepadAxis:
		return anyGamepad([&](std::size_t pad) { return input.wasReleased(binding.gamepadAxis, binding.direction, pad); });
	}
	return false;
}

double bindingValue(InputBinding const& binding, InputState const& input)
{
	if (binding.device != InputBinding::Device::GamepadAxis)
	{
		return bindingDown(binding, input) ? 1.0 : 0.0;
	}
	double best = 0.0;
	for (std::size_t pad = 0; pad < maxGamepads; ++pad)
	{
		double const raw = input.gamepadAxis(binding.gamepadAxis, pad);
		double const directed = binding.direction == AxisDirection::Positive ? raw : -raw;
		best = std::max(best, directed);
	}
	return best;
}

}

ActionId ActionMap::define(std::string_view const name)
{
	if (name.empty())
	{
		throw std::invalid_argument("ActionMap::define: action name must not be empty");
	}
	if (std::optional<ActionId> const existing = find(name))
	{
		return *existing;
	}
	actions_.push_back(Action {std::string {name}, {}});
	return ActionId {static_cast<std::uint32_t>(actions_.size() - 1)};
}

std::optional<ActionId> ActionMap::find(std::string_view const name) const
{
	for (std::size_t i = 0; i < actions_.size(); ++i)
	{
		if (actions_[i].name == name)
		{
			return ActionId {static_cast<std::uint32_t>(i)};
		}
	}
	return std::nullopt;
}

std::string_view ActionMap::name(ActionId const id) const
{
	return action(id).name;
}

void ActionMap::bind(ActionId const id, InputBinding const binding)
{
	if (id.value >= actions_.size())
	{
		throw std::out_of_range("ActionMap::bind: unknown action");
	}
	std::vector<InputBinding>& bindings = actions_[id.value].bindings;
	if (std::find(bindings.begin(), bindings.end(), binding) == bindings.end())
	{
		bindings.push_back(binding);
	}
}

ActionId ActionMap::bind(std::string_view const name, Key const key)
{
	ActionId const id = define(name);
	bind(id, InputBinding::fromKey(key));
	return id;
}

ActionId ActionMap::bind(std::string_view const name, MouseButton const button)
{
	ActionId const id = define(name);
	bind(id, InputBinding::fromMouse(button));
	return id;
}

ActionId ActionMap::bind(std::string_view const name, GamepadButton const button)
{
	ActionId const id = define(name);
	bind(id, InputBinding::fromGamepad(button));
	return id;
}

ActionId ActionMap::bind(std::string_view const name, GamepadAxis const axis, AxisDirection const direction)
{
	ActionId const id = define(name);
	bind(id, InputBinding::fromGamepadAxis(axis, direction));
	return id;
}

void ActionMap::clearBindings(ActionId const id)
{
	if (id.value >= actions_.size())
	{
		throw std::out_of_range("ActionMap::clearBindings: unknown action");
	}
	actions_[id.value].bindings.clear();
}

std::vector<InputBinding> const& ActionMap::bindings(ActionId const id) const
{
	return action(id).bindings;
}

bool ActionMap::isActive(ActionId const id, InputState const& input) const
{
	std::vector<InputBinding> const& list = action(id).bindings;
	return std::any_of(list.begin(), list.end(), [&input](InputBinding const& b)
		{
			return bindingDown(b, input);
		});
}

bool ActionMap::wasTriggered(ActionId const id, InputState const& input) const
{
	std::vector<InputBinding> const& list = action(id).bindings;
	return std::any_of(list.begin(), list.end(), [&input](InputBinding const& b)
		{
			return bindingPressed(b, input);
		});
}

bool ActionMap::wasReleased(ActionId const id, InputState const& input) const
{
	std::vector<InputBinding> const& list = action(id).bindings;
	return std::any_of(list.begin(), list.end(), [&input](InputBinding const& b)
		{
			return bindingReleased(b, input);
		});
}

double ActionMap::value(ActionId const id, InputState const& input) const
{
	double best = 0.0;
	for (InputBinding const& binding : action(id).bindings)
	{
		best = std::max(best, bindingValue(binding, input));
	}
	return best;
}

double ActionMap::axis(ActionId const negative, ActionId const positive, InputState const& input) const
{
	return std::clamp(value(positive, input) - value(negative, input), -1.0, 1.0);
}

ActionMap::Action const& ActionMap::action(ActionId const id) const
{
	if (id.value >= actions_.size())
	{
		throw std::out_of_range("ActionMap: unknown action");
	}
	return actions_[id.value];
}

}
