#pragma once

#include <glm/glm.hpp>

#include <cstdint>
#include <optional>

namespace lge
{

// Lightweight 2D collision geometry. It is deliberately not a rigid-body engine:
// it answers overlap, contact and ray queries, which is what top-down games need.
// Games requiring real dynamics should integrate Box2D (2D) or Jolt (3D) behind the
// same component interface instead of growing this module.

struct Aabb2
{
	glm::dvec2 min {0.0};
	glm::dvec2 max {0.0};

	[[nodiscard]] static Aabb2 fromCenter(glm::dvec2 const& center, glm::dvec2 const& halfExtents) noexcept
	{
		return Aabb2 {center - halfExtents, center + halfExtents};
	}

	[[nodiscard]] glm::dvec2 center() const noexcept
	{
		return (min + max) * 0.5;
	}
	[[nodiscard]] glm::dvec2 halfExtents() const noexcept
	{
		return (max - min) * 0.5;
	}
	[[nodiscard]] bool contains(glm::dvec2 const& point) const noexcept
	{
		return point.x >= min.x && point.x <= max.x && point.y >= min.y && point.y <= max.y;
	}
};

struct Circle
{
	glm::dvec2 center {0.0};
	double radius = 0.5;
};

struct Ray2
{
	glm::dvec2 origin {0.0};
	// Must be normalised; distances returned by ray queries are along this vector.
	glm::dvec2 direction {1.0, 0.0};
};

// Separation information for two overlapping shapes. Moving shape A by
// -normal * penetration (or B by +normal * penetration) separates them.
struct Contact2
{
	// Unit vector pointing from A towards B.
	glm::dvec2 normal {1.0, 0.0};
	double penetration = 0.0;
};

struct RayHit2
{
	double distance = 0.0;
	glm::dvec2 point {0.0};
	// Surface normal at the hit point, facing the ray origin.
	glm::dvec2 normal {0.0};
};

[[nodiscard]] bool overlaps(Aabb2 const& a, Aabb2 const& b) noexcept;
[[nodiscard]] bool overlaps(Circle const& a, Circle const& b) noexcept;
[[nodiscard]] bool overlaps(Circle const& circle, Aabb2 const& box) noexcept;

// Contacts only report genuine overlap; touching shapes produce no contact.
[[nodiscard]] std::optional<Contact2> collide(Circle const& a, Circle const& b) noexcept;
[[nodiscard]] std::optional<Contact2> collide(Aabb2 const& a, Aabb2 const& b) noexcept;
[[nodiscard]] std::optional<Contact2> collide(Circle const& a, Aabb2 const& b) noexcept;
[[nodiscard]] std::optional<Contact2> collide(Aabb2 const& a, Circle const& b) noexcept;

// Rays starting inside a shape report a hit at distance 0.
[[nodiscard]] std::optional<RayHit2> raycast(Ray2 const& ray, Circle const& circle, double maxDistance) noexcept;
[[nodiscard]] std::optional<RayHit2> raycast(Ray2 const& ray, Aabb2 const& box, double maxDistance) noexcept;

}
