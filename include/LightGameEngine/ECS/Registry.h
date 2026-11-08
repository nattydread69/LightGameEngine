#pragma once

#include "LightGameEngine/ECS/Entity.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <memory>
#include <tuple>
#include <span>
#include <stdexcept>
#include <string>
#include <typeinfo>
#include <utility>
#include <vector>

namespace lge
{

namespace detail
{

// Component types are numbered on first use so pools can live in a plain vector
// indexed by type, avoiding a hash lookup on every component access. The counter is
// process-wide; the numbering is not stable between runs and is never serialised.
inline std::size_t allocateComponentTypeId() noexcept
{
	static std::atomic<std::size_t> next {0};
	return next.fetch_add(1, std::memory_order_relaxed);
}

template <typename Component>
std::size_t componentTypeId() noexcept
{
	static std::size_t const id = allocateComponentTypeId();
	return id;
}

class ComponentPoolBase
{
public:
	ComponentPoolBase() = default;
	ComponentPoolBase(ComponentPoolBase const&) = delete;
	ComponentPoolBase& operator=(ComponentPoolBase const&) = delete;
	virtual ~ComponentPoolBase() = default;

	virtual bool remove(Entity entity) = 0;
	virtual void clear() = 0;

	[[nodiscard]] bool contains(Entity const entity) const noexcept
	{
		if (entity.index >= sparse_.size())
		{
			return false;
		}
		std::uint32_t const position = sparse_[entity.index];
		return position != absent && dense_[position] == entity;
	}

	[[nodiscard]] std::size_t size() const noexcept
	{
		return dense_.size();
	}
	[[nodiscard]] std::span<Entity const> entities() const noexcept
	{
		return dense_;
	}

protected:
	static constexpr std::uint32_t absent = std::numeric_limits<std::uint32_t>::max();

	// sparse_ maps an entity index to its position in the packed arrays; dense_ holds
	// the owning entity of each packed element.
	std::vector<std::uint32_t> sparse_;
	std::vector<Entity> dense_;
};

// Sparse-set storage: components are packed contiguously for cache-friendly
// iteration, and removal is O(1) by moving the last element into the gap.
template <typename Component>
class ComponentPool final : public ComponentPoolBase
{
public:
	template <typename... Args>
	Component& emplace(Entity const entity, Args&&... args)
	{
		if (contains(entity))
		{
			Component& existing = data_[sparse_[entity.index]];
			existing = Component {std::forward<Args>(args)...};
			return existing;
		}
		if (entity.index >= sparse_.size())
		{
			sparse_.resize(static_cast<std::size_t>(entity.index) + 1, absent);
		}
		data_.push_back(Component {std::forward<Args>(args)...});
		sparse_[entity.index] = static_cast<std::uint32_t>(dense_.size());
		dense_.push_back(entity);
		return data_.back();
	}

	bool remove(Entity const entity) override
	{
		if (!contains(entity))
		{
			return false;
		}
		std::uint32_t const position = sparse_[entity.index];
		std::uint32_t const last = static_cast<std::uint32_t>(dense_.size() - 1);
		if (position != last)
		{
			data_[position] = std::move(data_[last]);
			dense_[position] = dense_[last];
			sparse_[dense_[position].index] = position;
		}
		data_.pop_back();
		dense_.pop_back();
		sparse_[entity.index] = absent;
		return true;
	}

	void clear() override
	{
		data_.clear();
		dense_.clear();
		sparse_.clear();
	}

	[[nodiscard]] Component* tryGet(Entity const entity) noexcept
	{
		return contains(entity) ? &data_[sparse_[entity.index]] : nullptr;
	}

	[[nodiscard]] Component const* tryGet(Entity const entity) const noexcept
	{
		return contains(entity) ? &data_[sparse_[entity.index]] : nullptr;
	}

	[[nodiscard]] Component& atPosition(std::size_t const position) noexcept
	{
		return data_[position];
	}
	[[nodiscard]] std::span<Component> components() noexcept
	{
		return data_;
	}
	[[nodiscard]] std::span<Component const> components() const noexcept
	{
		return data_;
	}

private:
	std::vector<Component> data_;
};

}

// Owns entities and their components. Components are plain value types; any type
// that is movable and constructible from the given arguments can be attached.
//
// Structural rule: while iterating with each(), do not create or destroy entities
// and do not add or remove components of the types being iterated, because that
// can reallocate the storage the callback's references point into. Gameplay code
// should use GameWorld::queueDestroy() for deferred destruction instead.
class Registry
{
public:
	Registry() = default;
	Registry(Registry const&) = delete;
	Registry& operator=(Registry const&) = delete;
	Registry(Registry&&) noexcept = default;
	Registry& operator=(Registry&&) noexcept = default;
	~Registry() = default;

	Entity create()
	{
		std::uint32_t index = 0;
		if (!freeIndices_.empty())
		{
			index = freeIndices_.back();
			freeIndices_.pop_back();
		}
		else
		{
			index = static_cast<std::uint32_t>(generations_.size());
			generations_.push_back(1);
			alive_.push_back(false);
		}
		alive_[index] = true;
		++aliveCount_;
		return Entity {index, generations_[index]};
	}

	// Destroys the entity and all of its components. Returns false for entities that
	// are already dead or were never created by this registry.
	bool destroy(Entity const entity)
	{
		if (!isAlive(entity))
		{
			return false;
		}
		for (std::unique_ptr<detail::ComponentPoolBase> const& pool : pools_)
		{
			if (pool)
			{
				pool->remove(entity);
			}
		}
		alive_[entity.index] = false;
		std::uint32_t& generation = generations_[entity.index];
		// Generation 0 marks an invalid entity, so skip it on wrap-around.
		generation = generation == std::numeric_limits<std::uint32_t>::max() ? 1 : generation + 1;
		freeIndices_.push_back(entity.index);
		--aliveCount_;
		return true;
	}

	[[nodiscard]] bool isAlive(Entity const entity) const noexcept
	{
		return entity.index < generations_.size() && alive_[entity.index] &&
			generations_[entity.index] == entity.generation;
	}

	[[nodiscard]] std::size_t aliveCount() const noexcept
	{
		return aliveCount_;
	}

	// Adds the component, or replaces an existing one of the same type.
	template <typename Component, typename... Args>
	Component& emplace(Entity const entity, Args&&... args)
	{
		requireAlive(entity, "emplace");
		return pool<Component>().emplace(entity, std::forward<Args>(args)...);
	}

	template <typename Component>
	bool remove(Entity const entity)
	{
		detail::ComponentPool<Component>* const storage = findPool<Component>();
		return storage != nullptr && storage->remove(entity);
	}

	template <typename Component>
	[[nodiscard]] bool has(Entity const entity) const noexcept
	{
		detail::ComponentPool<Component> const* const storage = findPool<Component>();
		return storage != nullptr && storage->contains(entity);
	}

	template <typename Component>
	[[nodiscard]] Component* tryGet(Entity const entity) noexcept
	{
		detail::ComponentPool<Component>* const storage = findPool<Component>();
		return storage != nullptr ? storage->tryGet(entity) : nullptr;
	}

	template <typename Component>
	[[nodiscard]] Component const* tryGet(Entity const entity) const noexcept
	{
		detail::ComponentPool<Component> const* const storage = findPool<Component>();
		return storage != nullptr ? storage->tryGet(entity) : nullptr;
	}

	template <typename Component>
	[[nodiscard]] Component& get(Entity const entity)
	{
		Component* const component = tryGet<Component>(entity);
		if (component == nullptr)
		{
			throw std::out_of_range(std::string {"Registry::get: entity has no component "} + typeid(Component).name());
		}
		return *component;
	}

	template <typename Component>
	[[nodiscard]] Component const& get(Entity const entity) const
	{
		Component const* const component = tryGet<Component>(entity);
		if (component == nullptr)
		{
			throw std::out_of_range(std::string {"Registry::get: entity has no component "} + typeid(Component).name());
		}
		return *component;
	}

	template <typename Component>
	[[nodiscard]] std::size_t count() const noexcept
	{
		detail::ComponentPool<Component> const* const storage = findPool<Component>();
		return storage != nullptr ? storage->size() : 0;
	}

	// Calls function(Entity, First&, Rest&...) for every entity that has all of the
	// listed components. Iteration walks the smallest of the pools and tests the
	// others, so the cost is proportional to the rarest component.
	template <typename First, typename... Rest, typename Function>
	void each(Function&& function)
	{
		detail::ComponentPool<First>* const first = findPool<First>();
		if (first == nullptr)
		{
			return;
		}
		std::tuple<detail::ComponentPool<Rest>*...> const rest {findPool<Rest>()...};
		bool const allPresent = std::apply([](auto*... pools)
			{
				return ((pools != nullptr) && ...);
			}, rest);
		if (!allPresent)
		{
			return;
		}

		detail::ComponentPoolBase const* driver = first;
		std::apply([&driver](auto*... pools)
			{
				((pools->size() < driver->size() ? driver = pools : driver), ...);
			}, rest);

		std::span<Entity const> const entities = driver->entities();
		// Walking backwards keeps iteration valid if the callback removes the current
		// entity's components, which swaps the last element into the current slot.
		for (std::size_t i = entities.size(); i-- > 0;)
		{
			if (i >= driver->size())
			{
				continue;
			}
			Entity const entity = driver->entities()[i];
			First* const a = first->tryGet(entity);
			if (a == nullptr)
			{
				continue;
			}
			auto const others = std::apply([entity](auto*... pools)
				{
					return std::tuple {pools->tryGet(entity)...};
				}, rest);
			bool const complete = std::apply([](auto*... components)
				{
					return ((components != nullptr) && ...);
				}, others);
			if (!complete)
			{
				continue;
			}
			std::apply([&](auto*... components)
				{
					function(entity, *a, *components...);
				}, others);
		}
	}

	template <typename First, typename... Rest, typename Function>
	void each(Function&& function) const
	{
		// The const overload reuses the mutable traversal; it only hands out const
		// references, so the const_cast never permits modification.
		const_cast<Registry*>(this)->each<First, Rest...>(
			[&function](Entity const entity, First& a, Rest&... rest)
			{
				function(entity, static_cast<First const&>(a), static_cast<Rest const&>(rest)...);
			});
	}

	void clear()
	{
		for (std::unique_ptr<detail::ComponentPoolBase> const& pool : pools_)
		{
			if (pool)
			{
				pool->clear();
			}
		}
		for (std::size_t index = 0; index < alive_.size(); ++index)
		{
			if (alive_[index])
			{
				alive_[index] = false;
				std::uint32_t& generation = generations_[index];
				generation = generation == std::numeric_limits<std::uint32_t>::max() ? 1 : generation + 1;
				freeIndices_.push_back(static_cast<std::uint32_t>(index));
			}
		}
		aliveCount_ = 0;
	}

private:
	template <typename Component>
	detail::ComponentPool<Component>& pool()
	{
		std::size_t const id = detail::componentTypeId<Component>();
		if (id >= pools_.size())
		{
			pools_.resize(id + 1);
		}
		if (!pools_[id])
		{
			pools_[id] = std::make_unique<detail::ComponentPool<Component>>();
		}
		return static_cast<detail::ComponentPool<Component>&>(*pools_[id]);
	}

	template <typename Component>
	detail::ComponentPool<Component>* findPool() noexcept
	{
		std::size_t const id = detail::componentTypeId<Component>();
		return id < pools_.size() ? static_cast<detail::ComponentPool<Component>*>(pools_[id].get()) : nullptr;
	}

	template <typename Component>
	detail::ComponentPool<Component> const* findPool() const noexcept
	{
		std::size_t const id = detail::componentTypeId<Component>();
		return id < pools_.size() ? static_cast<detail::ComponentPool<Component> const*>(pools_[id].get()) : nullptr;
	}

	void requireAlive(Entity const entity, char const* const operation) const
	{
		if (!isAlive(entity))
		{
			throw std::invalid_argument(std::string {"Registry::"} + operation + ": entity is not alive");
		}
	}

	std::vector<std::uint32_t> generations_;
	std::vector<bool> alive_;
	std::vector<std::uint32_t> freeIndices_;
	std::size_t aliveCount_ = 0;
	std::vector<std::unique_ptr<detail::ComponentPoolBase>> pools_;
};

}
