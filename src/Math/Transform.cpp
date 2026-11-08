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

#include "LightGameEngine/Math/Transform.h"

#include <glm/gtc/matrix_transform.hpp>

#include <cmath>
#include <stdexcept>

namespace lge
{

glm::dmat4 Transform::matrix() const
{
	glm::dmat4 const translation = glm::translate(glm::dmat4 {1.0}, position);
	glm::dmat4 const rotationMatrix = glm::mat4_cast(rotation);
	glm::dmat4 const scaleMatrix = glm::scale(glm::dmat4 {1.0}, scale);
	return translation * rotationMatrix * scaleMatrix;
}

glm::dvec3 Transform::forward() const
{
	return rotation * worldForward;
}

glm::dvec3 Transform::right() const
{
	return rotation * worldRight;
}

glm::dvec3 Transform::up() const
{
	return rotation * worldUp;
}

Transform interpolate(Transform const& from, Transform const& to, double const alpha)
{
	Transform result;
	result.position = glm::mix(from.position, to.position, alpha);
	result.scale = glm::mix(from.scale, to.scale, alpha);
	result.rotation = glm::slerp(from.rotation, to.rotation, alpha);
	return result;
}

glm::dquat yawRotation(double const radians)
{
	return glm::angleAxis(radians, worldUp);
}

double yawFromDirection(glm::dvec3 const& direction)
{
	// Forward is -Z, and a positive yaw turns forward towards -X.
	return std::atan2(-direction.x, -direction.z);
}

glm::dquat lookRotation(glm::dvec3 const& forward, glm::dvec3 const& up)
{
	double const forwardLength = glm::length(forward);
	if (forwardLength < 1e-12)
	{
		throw std::invalid_argument("lookRotation: forward direction has zero length");
	}
	glm::dvec3 const f = forward / forwardLength;
	glm::dvec3 const r = glm::cross(f, up);
	double const rightLength = glm::length(r);
	if (rightLength < 1e-12)
	{
		throw std::invalid_argument("lookRotation: up is parallel to forward");
	}
	glm::dvec3 const right = r / rightLength;
	glm::dvec3 const trueUp = glm::cross(right, f);

	// Columns are where the local +X, +Y and +Z axes end up; local +Z is backwards.
	glm::dmat3 basis;
	basis[0] = right;
	basis[1] = trueUp;
	basis[2] = -f;
	return glm::normalize(glm::quat_cast(basis));
}

}
