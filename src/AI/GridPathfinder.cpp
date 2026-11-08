#include "LightGameEngine/AI/GridPathfinder.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace lge
{

GridMap::GridMap(int const width, int const height)
	: width_(width)
	, height_(height)
{
	if (width <= 0 || height <= 0)
	{
		throw std::invalid_argument("GridMap: dimensions must be positive");
	}
	blocked_.assign(static_cast<std::size_t>(width) * static_cast<std::size_t>(height), 0);
}

bool GridMap::inBounds(GridCell const& cell) const noexcept
{
	return cell.x >= 0 && cell.y >= 0 && cell.x < width_ && cell.y < height_;
}

bool GridMap::isBlocked(GridCell const& cell) const noexcept
{
	return !inBounds(cell) || blocked_[indexOf(cell)] != 0;
}

void GridMap::setBlocked(GridCell const& cell, bool const blocked)
{
	if (!inBounds(cell))
	{
		throw std::out_of_range("GridMap::setBlocked: cell is out of bounds");
	}
	blocked_[indexOf(cell)] = blocked ? 1 : 0;
}

void GridMap::fill(bool const blocked)
{
	std::fill(blocked_.begin(), blocked_.end(), static_cast<std::uint8_t>(blocked ? 1 : 0));
}

std::size_t GridMap::indexOf(GridCell const& cell) const noexcept
{
	return static_cast<std::size_t>(cell.y) * static_cast<std::size_t>(width_) + static_cast<std::size_t>(cell.x);
}

bool GridPathfinder::findPath(GridMap const& map, GridCell const start, GridCell const goal,
	std::vector<GridCell>& out, PathfindingOptions const& options)
{
	out.clear();
	lastExpanded_ = 0;
	if (map.isBlocked(start) || map.isBlocked(goal))
	{
		return false;
	}

	int const width = map.width();
	std::size_t const nodeCount = static_cast<std::size_t>(width) * static_cast<std::size_t>(map.height());
	if (cost_.size() != nodeCount)
	{
		cost_.assign(nodeCount, 0.0);
		parent_.assign(nodeCount, -1);
		visitStamp_.assign(nodeCount, 0);
		closed_.assign(nodeCount, 0);
		stamp_ = 0;
	}
	// A per-query stamp marks which entries are current, so the arrays need not be
	// cleared between queries. On wrap-around they are reset once.
	if (++stamp_ == 0)
	{
		std::fill(visitStamp_.begin(), visitStamp_.end(), 0u);
		stamp_ = 1;
	}

	auto const toNode = [width](GridCell const& c)
	{
		return static_cast<std::uint32_t>(c.y * width + c.x);
	};
	auto const toCell = [width](std::uint32_t const node)
	{
		return GridCell {static_cast<int>(node) % width, static_cast<int>(node) / width};
	};
	double const diagonalCost = std::sqrt(2.0);
	auto const heuristic = [&](GridCell const& c)
	{
		double const dx = std::abs(c.x - goal.x);
		double const dy = std::abs(c.y - goal.y);
		if (options.allowDiagonal)
		{
			return (dx + dy) + (diagonalCost - 2.0) * std::min(dx, dy);
		}
		return dx + dy;
	};
	auto const isCurrent = [this](std::uint32_t const node)
	{
		return visitStamp_[node] == stamp_;
	};
	auto const heapOrder = [](OpenEntry const& l, OpenEntry const& r)
	{
		return l.priority > r.priority;
	};

	open_.clear();
	std::uint32_t const startNode = toNode(start);
	std::uint32_t const goalNode = toNode(goal);
	visitStamp_[startNode] = stamp_;
	cost_[startNode] = 0.0;
	parent_[startNode] = -1;
	closed_[startNode] = 0;
	open_.push_back({heuristic(start), startNode});

	static constexpr std::array<GridCell, 8> directions {
		GridCell {1, 0}, GridCell {-1, 0}, GridCell {0, 1}, GridCell {0, -1},
		GridCell {1, 1}, GridCell {1, -1}, GridCell {-1, 1}, GridCell {-1, -1}};
	std::size_t const directionCount = options.allowDiagonal ? 8 : 4;

	while (!open_.empty())
	{
		std::pop_heap(open_.begin(), open_.end(), heapOrder);
		OpenEntry const current = open_.back();
		open_.pop_back();
		if (closed_[current.node] != 0)
		{
			// Stale duplicate entry; a cheaper route was already expanded.
			continue;
		}
		closed_[current.node] = 1;
		++lastExpanded_;

		if (current.node == goalNode)
		{
			for (std::int32_t node = static_cast<std::int32_t>(goalNode); node >= 0; node = parent_[static_cast<std::size_t>(node)])
			{
				out.push_back(toCell(static_cast<std::uint32_t>(node)));
			}
			std::reverse(out.begin(), out.end());
			return true;
		}

		GridCell const cell = toCell(current.node);
		for (std::size_t d = 0; d < directionCount; ++d)
		{
			GridCell const step = directions[d];
			GridCell const next = cell + step;
			if (map.isBlocked(next))
			{
				continue;
			}
			bool const diagonal = step.x != 0 && step.y != 0;
			if (diagonal && options.preventCornerCutting &&
				(map.isBlocked(GridCell {cell.x + step.x, cell.y}) || map.isBlocked(GridCell {cell.x, cell.y + step.y})))
			{
				continue;
			}
			std::uint32_t const nextNode = toNode(next);
			double const tentative = cost_[current.node] + (diagonal ? diagonalCost : 1.0);
			if (!isCurrent(nextNode))
			{
				visitStamp_[nextNode] = stamp_;
				closed_[nextNode] = 0;
			}
			else if (closed_[nextNode] != 0 || tentative >= cost_[nextNode])
			{
				continue;
			}
			cost_[nextNode] = tentative;
			parent_[nextNode] = static_cast<std::int32_t>(current.node);
			open_.push_back({tentative + heuristic(next), nextNode});
			std::push_heap(open_.begin(), open_.end(), heapOrder);
		}
	}
	return false;
}

}
