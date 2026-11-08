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

#include <cstdint>
#include <vector>

namespace lge
{

using GridCell = glm::ivec2;

// Rectangular occupancy grid for tile-based levels. Cell (0, 0) is the minimum
// corner; the mapping between cells and world space belongs to the game.
class GridMap
{
public:
	GridMap(int width, int height);

	[[nodiscard]] int width() const noexcept
	{
		return width_;
	}
	[[nodiscard]] int height() const noexcept
	{
		return height_;
	}
	[[nodiscard]] bool inBounds(GridCell const& cell) const noexcept;
	// Out-of-bounds cells count as blocked.
	[[nodiscard]] bool isBlocked(GridCell const& cell) const noexcept;
	void setBlocked(GridCell const& cell, bool blocked);
	void fill(bool blocked);

private:
	[[nodiscard]] std::size_t indexOf(GridCell const& cell) const noexcept;

	int width_;
	int height_;
	std::vector<std::uint8_t> blocked_;
};

struct PathfindingOptions
{
	bool allowDiagonal = true;
	// With diagonals enabled, refuse to squeeze between two blocked orthogonal
	// neighbours, so agents with a radius do not clip wall corners.
	bool preventCornerCutting = true;
};

// Finds a shortest path with A* (octile heuristic when diagonals are allowed,
// Manhattan otherwise). On success `out` holds the cells from start to goal
// inclusive and the function returns true. Scratch storage is kept in the object,
// so reusing one pathfinder avoids allocations across queries.
class GridPathfinder
{
public:
	bool findPath(GridMap const& map, GridCell start, GridCell goal, std::vector<GridCell>& out,
		PathfindingOptions const& options = {});

	// Nodes expanded by the most recent query, useful for profiling.
	[[nodiscard]] std::size_t lastExpandedCount() const noexcept
	{
		return lastExpanded_;
	}

private:
	struct OpenEntry
	{
		double priority;
		std::uint32_t node;
	};

	std::vector<double> cost_;
	std::vector<std::int32_t> parent_;
	std::vector<std::uint32_t> visitStamp_;
	std::vector<std::uint8_t> closed_;
	std::vector<OpenEntry> open_;
	std::uint32_t stamp_ = 0;
	std::size_t lastExpanded_ = 0;
};

}
