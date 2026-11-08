#include "LightGameEngine/Physics/Collision2D.h"

#include <algorithm>
#include <cmath>
#include <limits>

namespace lge
{

namespace
{

constexpr double epsilon = 1e-12;

}

bool overlaps(Aabb2 const& a, Aabb2 const& b) noexcept
{
	return a.min.x < b.max.x && a.max.x > b.min.x && a.min.y < b.max.y && a.max.y > b.min.y;
}

bool overlaps(Circle const& a, Circle const& b) noexcept
{
	double const radii = a.radius + b.radius;
	glm::dvec2 const d = b.center - a.center;
	return glm::dot(d, d) < radii * radii;
}

bool overlaps(Circle const& circle, Aabb2 const& box) noexcept
{
	glm::dvec2 const closest = glm::clamp(circle.center, box.min, box.max);
	glm::dvec2 const d = closest - circle.center;
	return glm::dot(d, d) < circle.radius * circle.radius;
}

std::optional<Contact2> collide(Circle const& a, Circle const& b) noexcept
{
	glm::dvec2 const d = b.center - a.center;
	double const distanceSquared = glm::dot(d, d);
	double const radii = a.radius + b.radius;
	if (distanceSquared >= radii * radii)
	{
		return std::nullopt;
	}
	double const distance = std::sqrt(distanceSquared);
	// Coincident centres have no meaningful direction; pick +X so callers still get
	// a usable separation.
	glm::dvec2 const normal = distance > epsilon ? d / distance : glm::dvec2 {1.0, 0.0};
	return Contact2 {normal, radii - distance};
}

std::optional<Contact2> collide(Aabb2 const& a, Aabb2 const& b) noexcept
{
	double const overlapX = std::min(a.max.x, b.max.x) - std::max(a.min.x, b.min.x);
	double const overlapY = std::min(a.max.y, b.max.y) - std::max(a.min.y, b.min.y);
	if (overlapX <= 0.0 || overlapY <= 0.0)
	{
		return std::nullopt;
	}
	glm::dvec2 const d = b.center() - a.center();
	// Separate along the axis of least penetration.
	if (overlapX < overlapY)
	{
		return Contact2 {{d.x < 0.0 ? -1.0 : 1.0, 0.0}, overlapX};
	}
	return Contact2 {{0.0, d.y < 0.0 ? -1.0 : 1.0}, overlapY};
}

std::optional<Contact2> collide(Circle const& a, Aabb2 const& b) noexcept
{
	glm::dvec2 const closest = glm::clamp(a.center, b.min, b.max);
	glm::dvec2 const d = closest - a.center;
	double const distanceSquared = glm::dot(d, d);

	if (distanceSquared > epsilon)
	{
		if (distanceSquared >= a.radius * a.radius)
		{
			return std::nullopt;
		}
		double const distance = std::sqrt(distanceSquared);
		return Contact2 {d / distance, a.radius - distance};
	}

	// The centre is inside the box: push the circle out through the nearest face.
	double const toLeft = a.center.x - b.min.x;
	double const toRight = b.max.x - a.center.x;
	double const toBottom = a.center.y - b.min.y;
	double const toTop = b.max.y - a.center.y;
	double const nearest = std::min({toLeft, toRight, toBottom, toTop});
	glm::dvec2 normal {1.0, 0.0};
	if (nearest == toLeft)
	{
		normal = {1.0, 0.0};
	}
	else if (nearest == toRight)
	{
		normal = {-1.0, 0.0};
	}
	else if (nearest == toBottom)
	{
		normal = {0.0, 1.0};
	}
	else
	{
		normal = {0.0, -1.0};
	}
	return Contact2 {normal, a.radius + nearest};
}

std::optional<Contact2> collide(Aabb2 const& a, Circle const& b) noexcept
{
	std::optional<Contact2> contact = collide(b, a);
	if (contact)
	{
		contact->normal = -contact->normal;
	}
	return contact;
}

std::optional<RayHit2> raycast(Ray2 const& ray, Circle const& circle, double const maxDistance) noexcept
{
	glm::dvec2 const m = ray.origin - circle.center;
	double const b = glm::dot(m, ray.direction);
	double const c = glm::dot(m, m) - circle.radius * circle.radius;
	if (c <= 0.0)
	{
		return RayHit2 {0.0, ray.origin, -ray.direction};
	}
	if (b > 0.0)
	{
		return std::nullopt;
	}
	double const discriminant = b * b - c;
	if (discriminant < 0.0)
	{
		return std::nullopt;
	}
	double const t = -b - std::sqrt(discriminant);
	if (t > maxDistance)
	{
		return std::nullopt;
	}
	glm::dvec2 const point = ray.origin + ray.direction * t;
	return RayHit2 {t, point, glm::normalize(point - circle.center)};
}

std::optional<RayHit2> raycast(Ray2 const& ray, Aabb2 const& box, double const maxDistance) noexcept
{
	if (box.contains(ray.origin))
	{
		return RayHit2 {0.0, ray.origin, -ray.direction};
	}

	double tMin = 0.0;
	double tMax = maxDistance;
	glm::dvec2 normal {0.0};
	for (int axis = 0; axis < 2; ++axis)
	{
		double const origin = ray.origin[axis];
		double const direction = ray.direction[axis];
		if (std::abs(direction) < epsilon)
		{
			if (origin < box.min[axis] || origin > box.max[axis])
			{
				return std::nullopt;
			}
			continue;
		}
		double t1 = (box.min[axis] - origin) / direction;
		double t2 = (box.max[axis] - origin) / direction;
		if (t1 > t2)
		{
			std::swap(t1, t2);
		}
		if (t1 > tMin)
		{
			tMin = t1;
			normal = glm::dvec2 {0.0};
			normal[axis] = direction > 0.0 ? -1.0 : 1.0;
		}
		tMax = std::min(tMax, t2);
		if (tMin > tMax)
		{
			return std::nullopt;
		}
	}
	return RayHit2 {tMin, ray.origin + ray.direction * tMin, normal};
}

}
