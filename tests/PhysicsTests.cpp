#include "TestHarness.h"

#include "LightGameEngine/AI/GridPathfinder.h"
#include "LightGameEngine/AI/Steering.h"
#include "LightGameEngine/Physics/Collision2D.h"
#include "LightGameEngine/Physics/CollisionWorld2D.h"

using namespace lge;
using namespace lge::test;

namespace
{

TestCase const aabbOverlap {"Collision2D: AABB overlap and contact", []
	{
		Aabb2 const a = Aabb2::fromCenter({0.0, 0.0}, {1.0, 1.0});
		Aabb2 const b = Aabb2::fromCenter({1.5, 0.2}, {1.0, 1.0});
		Aabb2 const far = Aabb2::fromCenter({5.0, 0.0}, {1.0, 1.0});
		Aabb2 const touching = Aabb2::fromCenter({2.0, 0.0}, {1.0, 1.0});
		check(overlaps(a, b), "overlapping boxes");
		check(!overlaps(a, far), "separate boxes");
		check(!overlaps(a, touching), "touching is not overlapping");
		std::optional<Contact2> const contact = collide(a, b);
		check(contact.has_value(), "contact");
		checkNear(contact->normal.x, 1.0, 0.0, "separate along X (smaller overlap)");
		checkNear(contact->penetration, 0.5, 1e-12, "penetration");
	}};

TestCase const circleOverlap {"Collision2D: circle-circle", []
	{
		Circle const a {{0.0, 0.0}, 1.0};
		Circle const b {{1.5, 0.0}, 1.0};
		std::optional<Contact2> const contact = collide(a, b);
		check(contact.has_value(), "overlap");
		checkNear(contact->penetration, 0.5, 1e-12, "penetration");
		checkNear(contact->normal.x, 1.0, 1e-12, "normal A to B");
		check(!collide(a, Circle {{2.0, 0.0}, 1.0}).has_value(), "touching");
		check(collide(a, a).has_value(), "coincident centres still separate");
	}};

TestCase const circleBox {"Collision2D: circle-box from outside and inside", []
	{
		Aabb2 const wall = Aabb2::fromCenter({0.0, 0.0}, {1.0, 1.0});
		std::optional<Contact2> const outside = collide(Circle {{1.3, 0.0}, 0.5}, wall);
		check(outside.has_value(), "overlap from the right");
		checkNear(outside->normal.x, -1.0, 1e-12, "normal points from circle into box");
		checkNear(outside->penetration, 0.2, 1e-12, "penetration");
		check(overlaps(Circle {{1.3, 0.0}, 0.5}, wall), "overlaps() agrees");

		std::optional<Contact2> const inside = collide(Circle {{0.8, 0.0}, 0.5}, wall);
		check(inside.has_value(), "centre inside");
		checkNear(inside->normal.x, -1.0, 1e-12, "pushed out through the nearest (right) face");
		checkNear(inside->penetration, 0.7, 1e-12, "radius + distance to face");

		std::optional<Contact2> const flipped = collide(wall, Circle {{1.3, 0.0}, 0.5});
		checkNear(flipped->normal.x, 1.0, 1e-12, "box-circle flips the normal");
		check(!collide(Circle {{1.6, 0.0}, 0.5}, wall).has_value(), "separate");
	}};

TestCase const rays {"Collision2D: ray intersection", []
	{
		Ray2 const ray {{-5.0, 0.0}, {1.0, 0.0}};
		std::optional<RayHit2> const circleHit = raycast(ray, Circle {{0.0, 0.0}, 1.0}, 100.0);
		check(circleHit.has_value(), "hits circle");
		checkNear(circleHit->distance, 4.0, 1e-12, "circle distance");
		checkNear(circleHit->normal.x, -1.0, 1e-12, "circle normal");
		check(!raycast(ray, Circle {{0.0, 0.0}, 1.0}, 3.0).has_value(), "beyond max distance");
		check(!raycast(Ray2 {{-5.0, 0.0}, {-1.0, 0.0}}, Circle {{0.0, 0.0}, 1.0}, 100.0).has_value(), "pointing away");

		Aabb2 const box = Aabb2::fromCenter({0.0, 0.0}, {1.0, 2.0});
		std::optional<RayHit2> const boxHit = raycast(ray, box, 100.0);
		check(boxHit.has_value(), "hits box");
		checkNear(boxHit->distance, 4.0, 1e-12, "box distance");
		checkNear(boxHit->normal.x, -1.0, 1e-12, "box normal");
		check(!raycast(Ray2 {{-5.0, 3.0}, {1.0, 0.0}}, box, 100.0).has_value(), "misses above");
		checkNear(raycast(Ray2 {{0.0, 0.0}, {1.0, 0.0}}, box, 10.0)->distance, 0.0, 0.0, "starting inside");
	}};

TestCase const worldPairs {"CollisionWorld2D: pair finding respects layers and statics", []
	{
		CollisionWorld2D world;
		CollisionBody2D wall;
		wall.shape = ColliderShape2D::box({1.0, 1.0});
		wall.isStatic = true;
		wall.layer = 0b001;
		world.add(wall);

		CollisionBody2D wall2 = wall;
		wall2.position = {1.5, 0.0};
		world.add(wall2);

		CollisionBody2D robot;
		robot.shape = ColliderShape2D::circle(0.5);
		robot.position = {1.2, 0.0};
		robot.layer = 0b010;
		world.add(robot);

		CollisionBody2D ghost = robot;
		ghost.layer = 0b100;
		ghost.mask = 0b100;
		world.add(ghost);

		std::vector<BodyPair> pairs;
		world.findOverlappingPairs(pairs);
		// robot touches both walls; static-static ignored; ghost masks everything else out.
		checkEqual(pairs.size(), std::size_t {2}, "two pairs");
		for (BodyPair const& pair : pairs)
		{
			check(pair.b == 2u, "each pair involves the robot");
		}
	}};

TestCase const worldQueries {"CollisionWorld2D: nearest raycast and overlap query", []
	{
		CollisionWorld2D world;
		CollisionBody2D near;
		near.userId = 1;
		near.position = {3.0, 0.0};
		world.add(near);
		CollisionBody2D far = near;
		far.userId = 2;
		far.position = {6.0, 0.0};
		world.add(far);
		CollisionBody2D trigger = near;
		trigger.userId = 3;
		trigger.position = {1.0, 0.0};
		trigger.isTrigger = true;
		world.add(trigger);

		std::optional<WorldRayHit2> const hit = world.raycast(Ray2 {{0.0, 0.0}, {1.0, 0.0}}, 100.0);
		check(hit.has_value() && world.body(hit->body).userId == 1, "nearest solid body, trigger ignored");
		std::optional<WorldRayHit2> const ignoring = world.raycast(Ray2 {{0.0, 0.0}, {1.0, 0.0}}, 100.0, allCollisionLayers, 1u);
		check(ignoring.has_value() && world.body(ignoring->body).userId == 2, "ignored body skipped");

		std::vector<std::uint32_t> found;
		world.queryOverlaps(ColliderShape2D::circle(1.0), {3.5, 0.0}, allCollisionLayers, found);
		checkEqual(found.size(), std::size_t {1}, "one body in range");
	}};

TestCase const astarFindsPath {"GridPathfinder: shortest path around a wall", []
	{
		GridMap map {7, 5};
		for (int y = 0; y < 4; ++y)
		{
			map.setBlocked({3, y}, true);
		}
		GridPathfinder pathfinder;
		std::vector<GridCell> path;
		check(pathfinder.findPath(map, {0, 0}, {6, 0}, path, PathfindingOptions {false, true}), "path found");
		check(path.front() == GridCell {0, 0} && path.back() == GridCell {6, 0}, "endpoints");
		checkEqual(path.size(), std::size_t {15}, "4-connected detour length: 14 moves");
		for (GridCell const& cell : path)
		{
			check(!map.isBlocked(cell), "path avoids walls");
		}
		for (std::size_t i = 1; i < path.size(); ++i)
		{
			GridCell const d = path[i] - path[i - 1];
			checkEqual(std::abs(d.x) + std::abs(d.y), 1, "orthogonal steps");
		}
	}};

TestCase const astarDiagonal {"GridPathfinder: diagonals and corner cutting", []
	{
		GridMap map {5, 5};
		GridPathfinder pathfinder;
		std::vector<GridCell> path;
		check(pathfinder.findPath(map, {0, 0}, {4, 4}, path), "open grid");
		checkEqual(path.size(), std::size_t {5}, "pure diagonal");

		GridMap corner {2, 2};
		corner.setBlocked({1, 0}, true);
		check(pathfinder.findPath(corner, {0, 0}, {1, 1}, path, PathfindingOptions {true, true}), "route around corner exists");
		checkEqual(path.size(), std::size_t {3}, "no corner cutting: goes around via (0, 1)");
		corner.setBlocked({0, 1}, true);
		check(!pathfinder.findPath(corner, {0, 0}, {1, 1}, path), "diagonal squeeze refused");
		check(pathfinder.findPath(corner, {0, 0}, {1, 1}, path, PathfindingOptions {true, false}), "allowed when cutting enabled");
	}};

TestCase const astarUnreachable {"GridPathfinder: unreachable and blocked endpoints", []
	{
		GridMap map {5, 5};
		for (int y = 0; y < 5; ++y)
		{
			map.setBlocked({2, y}, true);
		}
		GridPathfinder pathfinder;
		std::vector<GridCell> path;
		check(!pathfinder.findPath(map, {0, 0}, {4, 4}, path), "split map");
		check(path.empty(), "no path output");
		check(!pathfinder.findPath(map, {2, 2}, {0, 0}, path), "blocked start");
		check(!pathfinder.findPath(map, {0, 0}, {9, 9}, path), "out of bounds goal");
		// Reuse after failures still works.
		check(pathfinder.findPath(map, {0, 0}, {1, 4}, path), "reuse");
	}};

TestCase const steering {"Steering: seek, arrive and clamp", []
	{
		glm::dvec2 const v = seek({0.0, 0.0}, {10.0, 0.0}, 3.0);
		checkNear(v.x, 3.0, 1e-12, "seek full speed");
		glm::dvec2 const slow = arrive({0.0, 0.0}, {1.0, 0.0}, 4.0, 2.0);
		checkNear(slow.x, 2.0, 1e-12, "arrive slows inside radius");
		checkNear(glm::length(arrive({1.0, 1.0}, {1.0, 1.0}, 4.0, 2.0)), 0.0, 0.0, "stopped at target");
		checkNear(glm::length(clampLength({3.0, 4.0}, 2.5)), 2.5, 1e-12, "clamp");
		checkNear(flee({0.0, 0.0}, {1.0, 0.0}, 2.0).x, -2.0, 1e-12, "flee");
	}};

}
