#pragma once

#include <glm/glm.hpp>

namespace lge
{

// Stateless steering helpers producing desired velocities. Games combine and clamp
// them however their movement model requires; the engine does not impose an agent
// type or behaviour.

// Full speed straight towards the target.
[[nodiscard]] glm::dvec2 seek(glm::dvec2 const& position, glm::dvec2 const& target, double maxSpeed) noexcept;

// Like seek, but slows linearly inside slowingRadius and stops at the target.
[[nodiscard]] glm::dvec2 arrive(glm::dvec2 const& position, glm::dvec2 const& target, double maxSpeed,
	double slowingRadius) noexcept;

// Full speed directly away from the threat.
[[nodiscard]] glm::dvec2 flee(glm::dvec2 const& position, glm::dvec2 const& threat, double maxSpeed) noexcept;

// Limits a vector's length without changing its direction.
[[nodiscard]] glm::dvec2 clampLength(glm::dvec2 const& value, double maxLength) noexcept;

// Moves `current` towards `target` by at most maxDelta, e.g. for acceleration limits.
[[nodiscard]] glm::dvec2 moveTowards(glm::dvec2 const& current, glm::dvec2 const& target, double maxDelta) noexcept;

}
