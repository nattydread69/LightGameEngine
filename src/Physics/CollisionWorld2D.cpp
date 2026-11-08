#include "LightGameEngine/Physics/CollisionWorld2D.h"

#include <algorithm>
#include <numeric>

namespace lge
{

namespace
{

Circle asCircle(CollisionBody2D const& body) noexcept
{
	return Circle {body.position, body.shape.radius};
}

Aabb2 asBox(CollisionBody2D const& body) noexcept
{
	return Aabb2::fromCenter(body.position, body.shape.halfExtents);
}

}

Aabb2 ColliderShape2D::bounds(glm::dvec2 const& position) const noexcept
{
	if (kind == Kind::Circle)
	{
		return Aabb2::fromCenter(position, glm::dvec2 {radius});
	}
	return Aabb2::fromCenter(position, halfExtents);
}

bool layersInteract(CollisionBody2D const& a, CollisionBody2D const& b) noexcept
{
	return (a.mask & b.layer) != 0 && (b.mask & a.layer) != 0;
}

std::optional<Contact2> collide(CollisionBody2D const& a, CollisionBody2D const& b) noexcept
{
	using Kind = ColliderShape2D::Kind;
	if (a.shape.kind == Kind::Circle && b.shape.kind == Kind::Circle)
	{
		return collide(asCircle(a), asCircle(b));
	}
	if (a.shape.kind == Kind::Circle)
	{
		return collide(asCircle(a), asBox(b));
	}
	if (b.shape.kind == Kind::Circle)
	{
		return collide(asBox(a), asCircle(b));
	}
	return collide(asBox(a), asBox(b));
}

void CollisionWorld2D::clear() noexcept
{
	bodies_.clear();
	bounds_.clear();
}

std::uint32_t CollisionWorld2D::add(CollisionBody2D const& body)
{
	bodies_.push_back(body);
	bounds_.push_back(body.shape.bounds(body.position));
	return static_cast<std::uint32_t>(bodies_.size() - 1);
}

void CollisionWorld2D::findOverlappingPairs(std::vector<BodyPair>& out)
{
	out.clear();
	// Bodies may have moved since add(), e.g. after a resolution pass.
	for (std::size_t i = 0; i < bodies_.size(); ++i)
	{
		bounds_[i] = bodies_[i].shape.bounds(bodies_[i].position);
	}

	sortedByMinX_.resize(bodies_.size());
	std::iota(sortedByMinX_.begin(), sortedByMinX_.end(), 0u);
	std::sort(sortedByMinX_.begin(), sortedByMinX_.end(),
		[this](std::uint32_t const l, std::uint32_t const r)
			{
				return bounds_[l].min.x < bounds_[r].min.x;
			});

	for (std::size_t i = 0; i < sortedByMinX_.size(); ++i)
	{
		std::uint32_t const a = sortedByMinX_[i];
		for (std::size_t j = i + 1; j < sortedByMinX_.size(); ++j)
		{
			std::uint32_t const b = sortedByMinX_[j];
			// Everything further along starts to the right of A's extent.
			if (bounds_[b].min.x >= bounds_[a].max.x)
			{
				break;
			}
			CollisionBody2D const& bodyA = bodies_[a];
			CollisionBody2D const& bodyB = bodies_[b];
			if ((bodyA.isStatic && bodyB.isStatic) || !layersInteract(bodyA, bodyB) || !overlaps(bounds_[a], bounds_[b]))
			{
				continue;
			}
			// Report pairs in insertion order so results do not depend on sorting.
			std::uint32_t const first = std::min(a, b);
			std::uint32_t const second = std::max(a, b);
			if (std::optional<Contact2> const contact = collide(bodies_[first], bodies_[second]))
			{
				out.push_back(BodyPair {first, second, *contact});
			}
		}
	}
	std::sort(out.begin(), out.end(), [](BodyPair const& l, BodyPair const& r)
		{
			return l.a != r.a ? l.a < r.a : l.b < r.b;
		});
}

std::optional<WorldRayHit2> CollisionWorld2D::raycast(Ray2 const& ray, double const maxDistance,
	CollisionLayers const mask, std::optional<std::uint64_t> const ignoreUserId, bool const includeTriggers) const
{
	std::optional<WorldRayHit2> best;
	double limit = maxDistance;
	for (std::size_t i = 0; i < bodies_.size(); ++i)
	{
		CollisionBody2D const& body = bodies_[i];
		if ((body.layer & mask) == 0 || (body.isTrigger && !includeTriggers) ||
			(ignoreUserId && body.userId == *ignoreUserId))
		{
			continue;
		}
		std::optional<RayHit2> const hit = body.shape.kind == ColliderShape2D::Kind::Circle
			? lge::raycast(ray, asCircle(body), limit)
			: lge::raycast(ray, asBox(body), limit);
		if (hit && hit->distance <= limit)
		{
			limit = hit->distance;
			best = WorldRayHit2 {static_cast<std::uint32_t>(i), *hit};
		}
	}
	return best;
}

void CollisionWorld2D::queryOverlaps(ColliderShape2D const& shape, glm::dvec2 const& position,
	CollisionLayers const mask, std::vector<std::uint32_t>& out) const
{
	out.clear();
	CollisionBody2D probe;
	probe.position = position;
	probe.shape = shape;
	for (std::size_t i = 0; i < bodies_.size(); ++i)
	{
		if ((bodies_[i].layer & mask) != 0 && collide(probe, bodies_[i]))
		{
			out.push_back(static_cast<std::uint32_t>(i));
		}
	}
}

}
