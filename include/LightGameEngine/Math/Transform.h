#pragma once

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

namespace lge
{

// World coordinate convention, shared by every engine module:
//   right-handed, +Y is up, -Z is forward, +X is right.
// Matrices are column-major (GLM) and transform column vectors: world = M * local.
// A Transform applies scale first, then rotation, then translation.
// Top-down (2D/2.5D) games lie on the XZ ground plane; see Physics/Plane.h.
inline glm::dvec3 const worldUp {0.0, 1.0, 0.0};
inline glm::dvec3 const worldForward {0.0, 0.0, -1.0};
inline glm::dvec3 const worldRight {1.0, 0.0, 0.0};

// Authoritative spatial state of an entity. Double precision keeps positions exact
// far from the origin; the renderer converts to float relative to a render origin.
struct Transform
{
	glm::dvec3 position {0.0};
	glm::dquat rotation {1.0, 0.0, 0.0, 0.0};
	glm::dvec3 scale {1.0};

	[[nodiscard]] glm::dmat4 matrix() const;
	[[nodiscard]] glm::dvec3 forward() const;
	[[nodiscard]] glm::dvec3 right() const;
	[[nodiscard]] glm::dvec3 up() const;
};

// Linear interpolation of position and scale, shortest-path slerp of rotation.
[[nodiscard]] Transform interpolate(Transform const& from, Transform const& to, double alpha);

// Rotation about the world up axis. Positive angles turn counter-clockwise when
// viewed from above, so yaw 0 faces -Z and yaw +pi/2 faces -X.
[[nodiscard]] glm::dquat yawRotation(double radians);

// Yaw angle that makes a Transform face along the given ground-plane direction.
[[nodiscard]] double yawFromDirection(glm::dvec3 const& direction);

// Rotation whose forward (-Z) axis points along `forward` with its up (+Y) axis as
// close to `up` as possible. `up` must not be parallel to `forward`.
[[nodiscard]] glm::dquat lookRotation(glm::dvec3 const& forward, glm::dvec3 const& up);

}
