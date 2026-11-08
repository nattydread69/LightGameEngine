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
