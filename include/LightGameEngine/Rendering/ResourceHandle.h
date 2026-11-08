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
#include <limits>
#include <optional>
#include <utility>
#include <vector>

namespace lge
{

// Engine-level, backend-independent handle to a renderer resource. It is a plain
// value (index + generation) that never exposes GPU objects; the renderer that
// created it owns the resource. Destroying the resource bumps the generation, so a
// stale handle is detected instead of referring to a different resource.
template <typename Tag>
struct ResourceHandle
{
	std::uint32_t index = std::numeric_limits<std::uint32_t>::max();
	std::uint32_t generation = 0;

	[[nodiscard]] constexpr bool isValid() const noexcept
	{
		return index != std::numeric_limits<std::uint32_t>::max() && generation != 0;
	}

	friend constexpr bool operator==(ResourceHandle const&, ResourceHandle const&) noexcept = default;
};

struct MeshTag;
struct MaterialTag;
using MeshHandle = ResourceHandle<MeshTag>;
using MaterialHandle = ResourceHandle<MaterialTag>;

// Generational slot storage behind ResourceHandle. Shared by renderer backends so
// handle validation behaves identically everywhere.
template <typename Tag, typename Value>
class HandlePool
{
public:
	using Handle = ResourceHandle<Tag>;

	Handle insert(Value value)
	{
		std::uint32_t index = 0;
		if (!freeSlots_.empty())
		{
			index = freeSlots_.back();
			freeSlots_.pop_back();
		}
		else
		{
			index = static_cast<std::uint32_t>(slots_.size());
			slots_.push_back(Slot {});
		}
		Slot& slot = slots_[index];
		slot.value = std::move(value);
		++live_;
		return Handle {index, slot.generation};
	}

	// Returns the removed value so a backend can schedule its GPU resources for
	// release, or nullopt if the handle was stale.
	std::optional<Value> erase(Handle const handle)
	{
		Slot* const slot = find(handle);
		if (slot == nullptr)
		{
			return std::nullopt;
		}
		std::optional<Value> removed = std::move(slot->value);
		slot->value.reset();
		slot->generation = slot->generation == std::numeric_limits<std::uint32_t>::max() ? 1 : slot->generation + 1;
		freeSlots_.push_back(handle.index);
		--live_;
		return removed;
	}

	[[nodiscard]] Value* get(Handle const handle) noexcept
	{
		Slot* const slot = find(handle);
		return slot != nullptr ? &*slot->value : nullptr;
	}

	[[nodiscard]] Value const* get(Handle const handle) const noexcept
	{
		Slot const* const slot = find(handle);
		return slot != nullptr ? &*slot->value : nullptr;
	}

	[[nodiscard]] bool contains(Handle const handle) const noexcept
	{
		return find(handle) != nullptr;
	}
	[[nodiscard]] std::size_t size() const noexcept
	{
		return live_;
	}

	template <typename Function>
	void forEach(Function&& function)
	{
		for (std::size_t i = 0; i < slots_.size(); ++i)
		{
			if (slots_[i].value)
			{
				function(Handle {static_cast<std::uint32_t>(i), slots_[i].generation}, *slots_[i].value);
			}
		}
	}

private:
	struct Slot
	{
		std::optional<Value> value;
		std::uint32_t generation = 1;
	};

	Slot* find(Handle const handle) noexcept
	{
		if (handle.index >= slots_.size())
		{
			return nullptr;
		}
		Slot& slot = slots_[handle.index];
		return slot.value && slot.generation == handle.generation ? &slot : nullptr;
	}

	Slot const* find(Handle const handle) const noexcept
	{
		return const_cast<HandlePool*>(this)->find(handle);
	}

	std::vector<Slot> slots_;
	std::vector<std::uint32_t> freeSlots_;
	std::size_t live_ = 0;
};

}
