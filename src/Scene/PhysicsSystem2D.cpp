#include "LightGameEngine/Scene/PhysicsSystem2D.h"

#include <algorithm>

namespace lge
{

PhysicsSystem2D::PhysicsSystem2D(Config const& config)
	: config_(config)
{
}

void PhysicsSystem2D::step(GameWorld& world)
{
	collisionWorld_.clear();
	bodyEntities_.clear();
	events_.clear();

	world.each<TransformComponent, Collider2DComponent>(
		[this](Entity const entity, TransformComponent const& transform, Collider2DComponent const& collider)
		{
			CollisionBody2D body;
			body.userId = entity.id();
			body.position = toPlane(transform.position);
			body.shape = collider.shape;
			body.layer = collider.layer;
			body.mask = collider.mask;
			body.isStatic = collider.isStatic;
			body.isTrigger = collider.isTrigger;
			collisionWorld_.add(body);
			bodyEntities_.push_back(entity);
		});

	for (int iteration = 0; iteration < std::max(config_.iterations, 1); ++iteration)
	{
		collisionWorld_.findOverlappingPairs(pairs_);
		bool resolvedAny = false;
		for (BodyPair const& pair : pairs_)
		{
			CollisionBody2D& a = collisionWorld_.body(pair.a);
			CollisionBody2D& b = collisionWorld_.body(pair.b);
			bool const trigger = a.isTrigger || b.isTrigger;
			if (iteration == 0)
			{
				events_.push_back(CollisionEvent {bodyEntities_[pair.a], bodyEntities_[pair.b], pair.contact, trigger});
			}
			if (trigger)
			{
				continue;
			}
			glm::dvec2 const correction = pair.contact.normal * pair.contact.penetration;
			if (a.isStatic)
			{
				b.position += correction;
			}
			else if (b.isStatic)
			{
				a.position -= correction;
			}
			else
			{
				a.position -= correction * 0.5;
				b.position += correction * 0.5;
			}
			resolvedAny = true;
		}
		if (!resolvedAny)
		{
			break;
		}
	}

	for (std::size_t i = 0; i < bodyEntities_.size(); ++i)
	{
		CollisionBody2D const& body = collisionWorld_.body(static_cast<std::uint32_t>(i));
		if (body.isStatic)
		{
			continue;
		}
		TransformComponent& transform = world.get<TransformComponent>(bodyEntities_[i]);
		transform.position.x = body.position.x;
		transform.position.z = body.position.y;
	}
}

std::optional<EntityRayHit> PhysicsSystem2D::raycast(Ray2 const& ray, double const maxDistance,
	CollisionLayers const mask, Entity const ignore) const
{
	std::optional<std::uint64_t> const ignoreId = ignore.isValid() ? std::optional<std::uint64_t> {ignore.id()} : std::nullopt;
	std::optional<WorldRayHit2> const hit = collisionWorld_.raycast(ray, maxDistance, mask, ignoreId);
	if (!hit)
	{
		return std::nullopt;
	}
	return EntityRayHit {bodyEntities_[hit->body], hit->hit};
}

bool PhysicsSystem2D::hasLineOfSight(glm::dvec2 const& from, glm::dvec2 const& to, CollisionLayers const blockers,
	Entity const ignore) const
{
	glm::dvec2 const offset = to - from;
	double const distance = glm::length(offset);
	if (distance < 1e-9)
	{
		return true;
	}
	return !raycast(Ray2 {from, offset / distance}, distance, blockers, ignore).has_value();
}

void PhysicsSystem2D::queryOverlaps(ColliderShape2D const& shape, glm::dvec2 const& position,
	CollisionLayers const mask, std::vector<Entity>& out) const
{
	out.clear();
	collisionWorld_.queryOverlaps(shape, position, mask, queryScratch_);
	for (std::uint32_t const index : queryScratch_)
	{
		out.push_back(bodyEntities_[index]);
	}
}

}
