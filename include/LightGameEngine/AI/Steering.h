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
