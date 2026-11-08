#include "LightGameEngine/AI/Steering.h"

#include <algorithm>

namespace lge
{

glm::dvec2 seek(glm::dvec2 const& position, glm::dvec2 const& target, double const maxSpeed) noexcept
{
	glm::dvec2 const offset = target - position;
	double const distance = glm::length(offset);
	if (distance < 1e-12)
	{
		return glm::dvec2 {0.0};
	}
	return offset * (maxSpeed / distance);
}

glm::dvec2 arrive(glm::dvec2 const& position, glm::dvec2 const& target, double const maxSpeed,
	double const slowingRadius) noexcept
{
	glm::dvec2 const offset = target - position;
	double const distance = glm::length(offset);
	if (distance < 1e-12)
	{
		return glm::dvec2 {0.0};
	}
	double const speed = slowingRadius > 0.0 ? maxSpeed * std::min(distance / slowingRadius, 1.0) : maxSpeed;
	return offset * (speed / distance);
}

glm::dvec2 flee(glm::dvec2 const& position, glm::dvec2 const& threat, double const maxSpeed) noexcept
{
	return -seek(position, threat, maxSpeed);
}

glm::dvec2 clampLength(glm::dvec2 const& value, double const maxLength) noexcept
{
	double const length = glm::length(value);
	if (length <= maxLength || length < 1e-12)
	{
		return value;
	}
	return value * (maxLength / length);
}

glm::dvec2 moveTowards(glm::dvec2 const& current, glm::dvec2 const& target, double const maxDelta) noexcept
{
	return current + clampLength(target - current, maxDelta);
}

}
