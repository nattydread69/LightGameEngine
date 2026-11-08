#pragma once

#include <glm/glm.hpp>

namespace lge
{

// Top-down games simulate on the XZ ground plane: plane coordinate (u, v) is world
// (x, z), and height is world y. 2D physics uses plane coordinates throughout.
[[nodiscard]] inline glm::dvec2 toPlane(glm::dvec3 const& world) noexcept
{
	return glm::dvec2 {world.x, world.z};
}

[[nodiscard]] inline glm::dvec3 fromPlane(glm::dvec2 const& plane, double const height = 0.0) noexcept
{
	return glm::dvec3 {plane.x, height, plane.y};
}

}
