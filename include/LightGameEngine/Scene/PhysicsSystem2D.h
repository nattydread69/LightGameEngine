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

#include "LightGameEngine/Physics/CollisionWorld2D.h"
#include "LightGameEngine/Physics/Plane.h"
#include "LightGameEngine/Scene/GameWorld.h"

#include <optional>
#include <vector>

namespace lge
{

struct CollisionEvent
{
	Entity a;
	Entity b;
	// Normal points from a towards b, in plane coordinates.
	Contact2 contact;
	// True when either collider is a trigger (no separation was applied).
	bool trigger = false;
};

struct EntityRayHit
{
	Entity entity;
	RayHit2 hit;
};

// Binds the collision module to the ECS. Every tick it gathers entities with
// Transform + Collider2D, pushes dynamic colliders out of static ones and out of
// each other, writes corrected positions back, and records collision events for
// gameplay. It runs without any rendering.
class PhysicsSystem2D
{
public:
	struct Config
	{
		// Resolution passes per tick; more passes settle crowded contacts better.
		int iterations = 3;
	};

	PhysicsSystem2D() = default;
	explicit PhysicsSystem2D(Config const& config);

	void step(GameWorld& world);

	// Events from the most recent step. Entities may have been destroyed since, so
	// check GameWorld::isAlive before use.
	[[nodiscard]] std::vector<CollisionEvent> const& events() const noexcept
	{
		return events_;
	}

	// Queries against the bodies gathered by the most recent step.
	[[nodiscard]] std::optional<EntityRayHit> raycast(Ray2 const& ray, double maxDistance,
		CollisionLayers mask = allCollisionLayers, Entity ignore = nullEntity) const;
	[[nodiscard]] bool hasLineOfSight(glm::dvec2 const& from, glm::dvec2 const& to, CollisionLayers blockers,
		Entity ignore = nullEntity) const;
	void queryOverlaps(ColliderShape2D const& shape, glm::dvec2 const& position, CollisionLayers mask,
		std::vector<Entity>& out) const;

	[[nodiscard]] CollisionWorld2D const& collisionWorld() const noexcept
	{
		return collisionWorld_;
	}

private:
	Config config_;
	CollisionWorld2D collisionWorld_;
	std::vector<Entity> bodyEntities_;
	std::vector<BodyPair> pairs_;
	std::vector<CollisionEvent> events_;
	mutable std::vector<std::uint32_t> queryScratch_;
};

}
