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

#include "LightGameEngine/ECS/Registry.h"
#include "LightGameEngine/Rendering/RenderScene.h"
#include "LightGameEngine/Scene/Components.h"

#include <string_view>
#include <vector>

namespace lge
{

// Owns all gameplay entities and is the authority on simulation state. The renderer
// never sees entities: it receives RenderScene snapshots built by
// extractRenderScene(), so entities stay valid with rendering disabled.
class GameWorld
{
public:
	GameWorld() = default;
	GameWorld(GameWorld const&) = delete;
	GameWorld& operator=(GameWorld const&) = delete;

	// Creates an entity with a TransformComponent, and a NameComponent if a name is given.
	Entity createEntity(std::string_view name = {}, Transform const& transform = {});
	// Destroys immediately. Not allowed while iterating the registry; use queueDestroy.
	void destroyEntity(Entity entity);
	// Destroys at the end of the current simulation tick (see flushDestroyQueue).
	void queueDestroy(Entity entity);
	void flushDestroyQueue();
	[[nodiscard]] bool isAlive(Entity entity) const noexcept
	{
		return registry_.isAlive(entity);
	}
	[[nodiscard]] std::size_t entityCount() const noexcept
	{
		return registry_.aliveCount();
	}

	// First entity with the given name, or nullEntity. Linear in the number of names.
	[[nodiscard]] Entity findByName(std::string_view name) const;

	[[nodiscard]] Registry& registry() noexcept
	{
		return registry_;
	}
	[[nodiscard]] Registry const& registry() const noexcept
	{
		return registry_;
	}

	template <typename Component, typename... Args>
	Component& add(Entity const entity, Args&&... args)
	{
		return registry_.emplace<Component>(entity, std::forward<Args>(args)...);
	}

	template <typename Component>
	[[nodiscard]] Component& get(Entity const entity)
	{
		return registry_.get<Component>(entity);
	}
	template <typename Component>
	[[nodiscard]] Component const& get(Entity const entity) const
	{
		return registry_.get<Component>(entity);
	}
	template <typename Component>
	[[nodiscard]] Component* tryGet(Entity const entity) noexcept
	{
		return registry_.tryGet<Component>(entity);
	}
	template <typename Component>
	[[nodiscard]] bool has(Entity const entity) const noexcept
	{
		return registry_.has<Component>(entity);
	}

	template <typename... Components, typename Function>
	void each(Function&& function)
	{
		registry_.each<Components...>(std::forward<Function>(function));
	}
	template <typename... Components, typename Function>
	void each(Function&& function) const
	{
		registry_.each<Components...>(std::forward<Function>(function));
	}

	// The camera used for rendering. The entity needs Transform and Camera components.
	void setActiveCamera(Entity camera);
	[[nodiscard]] Entity activeCamera() const noexcept
	{
		return activeCamera_;
	}

	[[nodiscard]] RenderEnvironment& environment() noexcept
	{
		return environment_;
	}
	[[nodiscard]] RenderEnvironment const& environment() const noexcept
	{
		return environment_;
	}

	// Moves an entity without interpolating from its old position on the next frame.
	void teleport(Entity entity, Transform const& transform);

	// Attaches `child` to `parent` with the given local offset and snaps it into place.
	// Throws std::invalid_argument if the attachment would create a cycle.
	void attach(Entity child, Entity parent, Transform const& local = {});
	void detach(Entity child);

	// Simulation steps, called by the engine once per tick in this order.
	void storeTransformHistory();
	void integrateVelocities(double stepSeconds);
	// Recomputes attached entities' world transforms from their parents.
	void updateAttachments();

	// Builds this frame's render snapshot. `alpha` in [0, 1] interpolates between the
	// previous and current tick's transforms; pass 1 to render the latest state.
	void extractRenderScene(RenderScene& scene, double alpha) const;

	// Destroys every entity.
	void clear();

private:
	Registry registry_;
	std::vector<Entity> destroyQueue_;
	Entity activeCamera_;
	RenderEnvironment environment_;
	std::vector<std::uint32_t> attachmentStamp_;
	std::uint32_t attachmentPass_ = 0;
};

}
