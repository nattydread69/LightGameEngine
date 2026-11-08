#include "TestHarness.h"

#include "LightGameEngine/ECS/Registry.h"

#include <memory>
#include <set>

using namespace lge;
using namespace lge::test;

namespace
{

struct Position
{
	double x = 0.0;
	double y = 0.0;
};

struct Health
{
	int value = 100;
};

struct Tag
{
};

TestCase const createAndDestroy {"Registry: create and destroy entities", []
	{
		Registry registry;
		Entity const a = registry.create();
		Entity const b = registry.create();
		check(a.isValid() && b.isValid(), "created entities are valid");
		check(a != b, "distinct identifiers");
		checkEqual(registry.aliveCount(), std::size_t {2}, "two alive");
		check(registry.destroy(a), "destroy succeeds");
		check(!registry.isAlive(a), "destroyed entity is dead");
		check(!registry.destroy(a), "double destroy is rejected");
		checkEqual(registry.aliveCount(), std::size_t {1}, "one alive");
		check(!registry.isAlive(nullEntity), "null entity is never alive");
	}};

TestCase const staleHandlesDetected {"Registry: recycled slots invalidate stale handles", []
	{
		Registry registry;
		Entity const first = registry.create();
		registry.emplace<Health>(first, 10);
		registry.destroy(first);
		Entity const second = registry.create();
		checkEqual(second.index, first.index, "slot is reused");
		check(second.generation != first.generation, "generation differs");
		check(!registry.isAlive(first), "stale handle is not alive");
		check(registry.tryGet<Health>(first) == nullptr, "stale handle sees no component");
		check(!registry.has<Health>(second), "new entity does not inherit components");
		checkThrows<std::invalid_argument>([&]
			{
				registry.emplace<Health>(first, 1);
			}, "emplace on dead entity throws");
	}};

TestCase const addRemoveLookup {"Registry: add, replace, look up and remove components", []
	{
		Registry registry;
		Entity const e = registry.create();
		registry.emplace<Position>(e, 1.0, 2.0);
		check(registry.has<Position>(e), "has position");
		check(!registry.has<Health>(e), "no health yet");
		checkNear(registry.get<Position>(e).y, 2.0, 0.0, "value stored");
		registry.emplace<Position>(e, 5.0, 6.0);
		checkNear(registry.get<Position>(e).x, 5.0, 0.0, "emplace replaces");
		checkEqual(registry.count<Position>(), std::size_t {1}, "still one component");
		check(registry.remove<Position>(e), "remove succeeds");
		check(!registry.remove<Position>(e), "second remove reports false");
		check(registry.tryGet<Position>(e) == nullptr, "removed");
		checkThrows<std::out_of_range>([&]
			{
				(void)registry.get<Position>(e);
			}, "get of missing component throws");
	}};

TestCase const destroyRemovesComponents {"Registry: destroying an entity removes all its components", []
	{
		Registry registry;
		Entity const a = registry.create();
		Entity const b = registry.create();
		registry.emplace<Position>(a);
		registry.emplace<Health>(a);
		registry.emplace<Position>(b, 7.0, 7.0);
		registry.destroy(a);
		checkEqual(registry.count<Position>(), std::size_t {1}, "one position left");
		checkEqual(registry.count<Health>(), std::size_t {0}, "no health left");
		checkNear(registry.get<Position>(b).x, 7.0, 0.0, "survivor's data intact after swap-remove");
	}};

TestCase const queryMultiple {"Registry: each() visits only entities with all components", []
	{
		Registry registry;
		std::set<std::uint32_t> expected;
		for (int i = 0; i < 10; ++i)
		{
			Entity const e = registry.create();
			registry.emplace<Position>(e, double(i), 0.0);
			if (i % 2 == 0)
			{
				registry.emplace<Health>(e, i);
				expected.insert(e.index);
			}
			if (i % 3 == 0)
			{
				registry.emplace<Tag>(e);
			}
		}
		std::set<std::uint32_t> visited;
		registry.each<Position, Health>([&](Entity const e, Position& p, Health& h)
			{
				visited.insert(e.index);
				check(static_cast<int>(p.x) == h.value, "components belong to the same entity");
			});
		check(visited == expected, "visited exactly the even entities");

		int both = 0;
		registry.each<Position, Health, Tag>([&](Entity, Position&, Health&, Tag&)
			{
				++both;
			});
		checkEqual(both, 2, "entities 0 and 6 have all three");

		int none = 0;
		registry.each<Position, double>([&](Entity, Position&, double&)
			{
				++none;
			});
		checkEqual(none, 0, "unused component type yields nothing");
	}};

TestCase const iterationMutatesInPlace {"Registry: each() gives mutable references", []
	{
		Registry registry;
		Entity const e = registry.create();
		registry.emplace<Health>(e, 10);
		registry.each<Health>([](Entity, Health& h)
			{
				h.value += 5;
			});
		checkEqual(registry.get<Health>(e).value, 15, "modified through iteration");
		Registry const& view = registry;
		int sum = 0;
		view.each<Health>([&](Entity, Health const& h)
			{
				sum += h.value;
			});
		checkEqual(sum, 15, "const iteration");
	}};

TestCase const removeDuringIteration {"Registry: removing the current entity's component during each() is safe", []
	{
		Registry registry;
		for (int i = 0; i < 6; ++i)
		{
			registry.emplace<Health>(registry.create(), i);
		}
		int visited = 0;
		registry.each<Health>([&](Entity const e, Health&)
			{
				++visited;
				registry.remove<Health>(e);
			});
		checkEqual(visited, 6, "every entity visited once");
		checkEqual(registry.count<Health>(), std::size_t {0}, "all removed");
	}};

TestCase const moveOnlyComponents {"Registry: move-only components are supported", []
	{
		Registry registry;
		Entity const e = registry.create();
		registry.emplace<std::unique_ptr<int>>(e, std::make_unique<int>(42));
		Entity const f = registry.create();
		registry.emplace<std::unique_ptr<int>>(f, std::make_unique<int>(7));
		registry.destroy(e);
		checkEqual(**registry.tryGet<std::unique_ptr<int>>(f), 7, "moved into the gap intact");
	}};

TestCase const clearResets {"Registry: clear() destroys everything", []
	{
		Registry registry;
		Entity const e = registry.create();
		registry.emplace<Health>(e);
		registry.clear();
		checkEqual(registry.aliveCount(), std::size_t {0}, "none alive");
		check(!registry.isAlive(e), "old handle dead");
		checkEqual(registry.count<Health>(), std::size_t {0}, "components gone");
	}};

TestCase const entityIdRoundTrip {"Entity: id() and fromId() round-trip", []
	{
		Entity const e {12345u, 678u};
		check(Entity::fromId(e.id()) == e, "round trip");
	}};

}
