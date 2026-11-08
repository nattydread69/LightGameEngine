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

#include "LightGameEngine/Physics/Collision2D.h"

#include <cstdint>
#include <optional>
#include <vector>

namespace lge
{

using CollisionLayers = std::uint32_t;
inline constexpr CollisionLayers allCollisionLayers = 0xffffffffu;

// Collision shape in body-local space, centred on the body position.
struct ColliderShape2D
{
	enum class Kind : std::uint8_t
	{
		Circle,
		Box
	};

	Kind kind = Kind::Circle;
	double radius = 0.5;
	glm::dvec2 halfExtents {0.5};

	[[nodiscard]] static ColliderShape2D circle(double const radius) noexcept
	{
		return ColliderShape2D {Kind::Circle, radius, glm::dvec2 {radius}};
	}

	[[nodiscard]] static ColliderShape2D box(glm::dvec2 const& halfExtents) noexcept
	{
		return ColliderShape2D {Kind::Box, 0.0, halfExtents};
	}

	[[nodiscard]] Aabb2 bounds(glm::dvec2 const& position) const noexcept;
};

struct CollisionBody2D
{
	// Opaque identifier chosen by the caller, e.g. a packed Entity id.
	std::uint64_t userId = 0;
	glm::dvec2 position {0.0};
	ColliderShape2D shape;
	// The layers this body is on, and the layers it collides with. Two bodies
	// interact only if each one's mask accepts the other's layer.
	CollisionLayers layer = 1;
	CollisionLayers mask = allCollisionLayers;
	bool isStatic = false;
	bool isTrigger = false;
};

struct BodyPair
{
	std::uint32_t a = 0;
	std::uint32_t b = 0;
	Contact2 contact;
};

struct WorldRayHit2
{
	std::uint32_t body = 0;
	RayHit2 hit;
};

[[nodiscard]] std::optional<Contact2> collide(CollisionBody2D const& a, CollisionBody2D const& b) noexcept;
[[nodiscard]] bool layersInteract(CollisionBody2D const& a, CollisionBody2D const& b) noexcept;

// A flat set of collision bodies rebuilt by its owner (usually every tick) and
// queried for overlapping pairs, rays and areas. Pair finding uses sort-and-sweep
// along X, which is close to linear for the sparse scenes typical of top-down games.
class CollisionWorld2D
{
public:
	void clear() noexcept;
	std::uint32_t add(CollisionBody2D const& body);
	[[nodiscard]] std::size_t size() const noexcept
	{
		return bodies_.size();
	}
	[[nodiscard]] CollisionBody2D const& body(std::uint32_t index) const
	{
		return bodies_.at(index);
	}
	CollisionBody2D& body(std::uint32_t index)
	{
		return bodies_.at(index);
	}

	// Appends every interacting, genuinely overlapping pair. Static-static pairs are
	// skipped. `out` is cleared first and reused to avoid per-tick allocation.
	void findOverlappingPairs(std::vector<BodyPair>& out);

	// Nearest hit among bodies on layers accepted by `mask`; triggers are ignored
	// unless includeTriggers is set. `ignoreUserId` excludes e.g. the caster itself.
	[[nodiscard]] std::optional<WorldRayHit2> raycast(Ray2 const& ray, double maxDistance,
		CollisionLayers mask = allCollisionLayers, std::optional<std::uint64_t> ignoreUserId = std::nullopt,
		bool includeTriggers = false) const;

	// Indices of bodies on layers accepted by `mask` that overlap the given shape.
	void queryOverlaps(ColliderShape2D const& shape, glm::dvec2 const& position, CollisionLayers mask,
		std::vector<std::uint32_t>& out) const;

private:
	std::vector<CollisionBody2D> bodies_;
	std::vector<Aabb2> bounds_;
	std::vector<std::uint32_t> sortedByMinX_;
};

}
