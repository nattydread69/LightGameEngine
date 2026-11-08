#pragma once

#include <cstdint>
#include <functional>
#include <limits>

namespace lge
{

// Lightweight entity identifier: a slot index plus a generation counter. Destroying an
// entity bumps its slot's generation, so stale copies of the identifier are detected
// instead of silently aliasing a newer entity that reuses the slot.
struct Entity
{
	static constexpr std::uint32_t invalidIndex = std::numeric_limits<std::uint32_t>::max();

	std::uint32_t index = invalidIndex;
	std::uint32_t generation = 0;

	[[nodiscard]] constexpr bool isValid() const noexcept
	{
		return index != invalidIndex && generation != 0;
	}

	// Packs the identifier into 64 bits, e.g. as a stable key for the renderer.
	[[nodiscard]] constexpr std::uint64_t id() const noexcept
	{
		return (static_cast<std::uint64_t>(generation) << 32) | index;
	}

	[[nodiscard]] static constexpr Entity fromId(std::uint64_t const id) noexcept
	{
		return Entity {static_cast<std::uint32_t>(id & 0xffffffffu), static_cast<std::uint32_t>(id >> 32)};
	}

	friend constexpr bool operator==(Entity const&, Entity const&) noexcept = default;
};

inline constexpr Entity nullEntity {};

}

template <>
struct std::hash<lge::Entity>
{
	std::size_t operator()(lge::Entity const& entity) const noexcept
	{
		return std::hash<std::uint64_t> {}(entity.id());
	}
};
