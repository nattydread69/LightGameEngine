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

#include "LightGameEngine/Core/FixedTimestep.h"
#include "LightGameEngine/Core/GameClock.h"
#include "LightGameEngine/Input/ActionMap.h"
#include "LightGameEngine/Input/InputState.h"
#include "LightGameEngine/Platform/IPlatform.h"
#include "LightGameEngine/Rendering/IRenderer.h"
#include "LightGameEngine/Rendering/RenderScene.h"
#include "LightGameEngine/Scene/GameWorld.h"
#include "LightGameEngine/Scene/PhysicsSystem2D.h"

#include <cstdint>
#include <string>

namespace lge
{

class GameEngine;

struct EngineConfig
{
	std::string applicationName = "LightGameEngine";
	FixedTimestepConfig timestep;
	// Interpolate rendered transforms between the last two ticks. When off, frames
	// show the latest simulated state.
	bool interpolate = true;
	PhysicsSystem2D::Config physics;
};

// Game-specific behaviour plugs in here. All callbacks run on the main thread.
class Application
{
public:
	Application() = default;
	Application(Application const&) = delete;
	Application& operator=(Application const&) = delete;
	virtual ~Application() = default;

	// After the engine is ready, before the first frame: create entities and resources.
	virtual void onStart(GameEngine& engine);
	// Fixed-rate simulation. Runs zero or more times per frame, always with the same
	// step length. Gameplay rules belong here. Collision events from the previous
	// tick are available through engine.physics().events().
	virtual void onFixedUpdate(GameEngine& engine, double stepSeconds) = 0;
	// Once per rendered frame after simulation, at a variable rate. For presentation
	// only (camera smoothing, HUD text); must not change gameplay state.
	virtual void onFrame(GameEngine& engine, double frameSeconds);
	// After the world's render snapshot is built, to add overlays such as HUD text.
	virtual void onBuildRenderScene(GameEngine& engine, RenderScene& scene);
	// Before the engine releases the world. Destroy renderer resources here.
	virtual void onShutdown(GameEngine& engine);
};

// Runs an Application: owns the world, input, physics and clock, and borrows the
// platform and (optional) renderer, which must outlive it.
//
// Per frame:  poll input -> resize -> N fixed ticks -> onFrame -> extract -> render.
// Per tick:   publish input edges -> store transform history -> onFixedUpdate ->
//             integrate velocities -> 2D physics -> attachments -> flush deferred destruction.
class GameEngine
{
public:
	// Pass renderer = nullptr for pure headless simulation.
	GameEngine(EngineConfig config, IPlatform& platform, IRenderer* renderer);
	GameEngine(GameEngine const&) = delete;
	GameEngine& operator=(GameEngine const&) = delete;
	~GameEngine();

	// Starts the application, runs the platform loop until it ends, then shuts down.
	// Exceptions from the application still shut down cleanly before propagating.
	void run(Application& application);

	// Lower-level lifecycle for tests and custom loops.
	void start(Application& application);
	// One frame at the given time from the platform clock; false once quit is requested.
	bool frame(double nowSeconds);
	// Runs exactly `ticks` simulation steps with no rendering or real time, for
	// deterministic headless simulation.
	void stepSimulation(std::uint32_t ticks);
	void shutdown();

	void requestQuit() noexcept
	{
		quitRequested_ = true;
	}
	[[nodiscard]] bool quitRequested() const noexcept
	{
		return quitRequested_;
	}
	[[nodiscard]] bool isRunning() const noexcept
	{
		return application_ != nullptr;
	}

	[[nodiscard]] GameWorld& world() noexcept
	{
		return world_;
	}
	[[nodiscard]] GameWorld const& world() const noexcept
	{
		return world_;
	}
	[[nodiscard]] InputState const& input() const noexcept
	{
		return input_;
	}
	[[nodiscard]] ActionMap& actions() noexcept
	{
		return actions_;
	}
	[[nodiscard]] ActionMap const& actions() const noexcept
	{
		return actions_;
	}
	[[nodiscard]] PhysicsSystem2D& physics() noexcept
	{
		return physics_;
	}
	[[nodiscard]] PhysicsSystem2D const& physics() const noexcept
	{
		return physics_;
	}
	// Null when running without a renderer.
	[[nodiscard]] IRenderer* renderer() noexcept
	{
		return renderer_;
	}
	[[nodiscard]] IPlatform& platform() noexcept
	{
		return *platform_;
	}
	[[nodiscard]] FrameTiming const& timing() const noexcept
	{
		return clock_.timing();
	}
	[[nodiscard]] FixedStepResult const& lastFrameSteps() const noexcept
	{
		return lastSteps_;
	}
	[[nodiscard]] WindowExtent extent() const noexcept
	{
		return extent_;
	}
	[[nodiscard]] EngineConfig const& config() const noexcept
	{
		return config_;
	}
	[[nodiscard]] RenderScene const& lastRenderScene() const noexcept
	{
		return renderScene_;
	}

	// Input injection for tests and tools; normal input arrives via the platform.
	[[nodiscard]] InputState& mutableInput() noexcept
	{
		return input_;
	}

private:
	void tick();

	EngineConfig config_;
	IPlatform* platform_;
	IRenderer* renderer_;
	Application* application_ = nullptr;

	GameWorld world_;
	InputState input_;
	ActionMap actions_;
	PhysicsSystem2D physics_;
	GameClock clock_;
	FixedTimestep timestep_;
	FixedStepResult lastSteps_;
	RenderScene renderScene_;
	WindowExtent extent_;
	bool quitRequested_ = false;
};

}
