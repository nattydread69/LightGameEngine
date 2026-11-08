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

bool bindingDown(InputBinding const& binding, InputState const& input)
{
	return binding.device == InputBinding::Device::Keyboard ? input.isDown(binding.key) : input.isDown(binding.button);
}

bool bindingPressed(InputBinding const& binding, InputState const& input)
{
	return binding.device == InputBinding::Device::Keyboard ? input.wasPressed(binding.key) : input.wasPressed(binding.button);
}

bool bindingReleased(InputBinding const& binding, InputState const& input)
{
	return binding.device == InputBinding::Device::Keyboard ? input.wasReleased(binding.key) : input.wasReleased(binding.button);
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

double ActionMap::axis(ActionId const negative, ActionId const positive, InputState const& input) const
{
	double value = 0.0;
	if (isActive(negative, input))
	{
		value -= 1.0;
	}
	if (isActive(positive, input))
	{
		value += 1.0;
	}
	return value;
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
