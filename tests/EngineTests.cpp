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

#include "TestHarness.h"

#include "LightGameEngine/Assets/AssetManager.h"
#include "LightGameEngine/Backends/NullRenderer.h"
#include "LightGameEngine/Engine/GameEngine.h"
#include "LightGameEngine/Platform/HeadlessPlatform.h"

#include <filesystem>
#include <fstream>

using namespace lge;
using namespace lge::test;

namespace
{

class CountingApp final : public Application
{
public:
	void onStart(GameEngine& engine) override
	{
		++starts;
		fire = engine.actions().bind("Fire", Key::Space);
		mesh = engine.renderer()->createMesh(MeshDescription::primitive(PrimitiveShape::Cube));
		material = engine.renderer()->createMaterial(MaterialDescription {});
		mover = engine.world().createEntity("Mover");
		engine.world().add<MeshRendererComponent>(mover, MeshRendererComponent {mesh, material, true});
		engine.world().add<VelocityComponent>(mover, VelocityComponent {{1.0, 0.0, 0.0}, {}});
	}

	void onFixedUpdate(GameEngine& engine, double const step) override
	{
		++ticks;
		stepSeen = step;
		if (engine.actions().wasTriggered(fire, engine.input()))
		{
			++fired;
		}
		if (quitAfterTicks > 0 && ticks >= quitAfterTicks)
		{
			engine.requestQuit();
		}
	}

	void onFrame(GameEngine&, double) override
	{
		++frames;
	}

	void onShutdown(GameEngine& engine) override
	{
		++shutdowns;
		engine.renderer()->destroyMesh(mesh);
		engine.renderer()->destroyMaterial(material);
	}

	int starts = 0;
	int ticks = 0;
	int frames = 0;
	int fired = 0;
	int shutdowns = 0;
	int quitAfterTicks = 0;
	double stepSeen = 0.0;
	ActionId fire;
	MeshHandle mesh;
	MaterialHandle material;
	Entity mover;
};

class ThrowingApp final : public Application
{
public:
	void onFixedUpdate(GameEngine&, double) override
	{
		throw std::runtime_error("gameplay failure");
	}

	void onShutdown(GameEngine&) override
	{
		shutDown = true;
	}

	bool shutDown = false;
};

TestCase const engineLifecycle {"GameEngine: initialisation, run and shutdown", []
	{
		HeadlessPlatform platform {HeadlessPlatform::Config {1.0 / 60.0, 120, {640, 480}}};
		NullRenderer renderer;
		CountingApp app;
		{
			GameEngine engine {EngineConfig {}, platform, &renderer};
			engine.run(app);
			check(!engine.isRunning(), "stopped after run");
			checkEqual(engine.world().entityCount(), std::size_t {0}, "world cleared at shutdown");
		}
		checkEqual(app.starts, 1, "started once");
		checkEqual(app.shutdowns, 1, "shut down once");
		checkEqual(app.frames, 120, "one onFrame per platform frame");
		// The first frame has zero elapsed time, so 119 frame intervals elapse.
		checkEqual(app.ticks, 119, "60 Hz simulation over 119 intervals of 1/60 s");
		checkEqual(renderer.statistics().framesRendered, std::uint64_t {120}, "rendered every frame");
		checkEqual(renderer.width(), 640u, "renderer sized from the platform");
		checkEqual(renderer.statistics().meshes, std::size_t {0}, "app released its resources");
	}};

TestCase const simulationIndependentOfFrameRate {"GameEngine: simulation is independent of the render rate", []
	{
		auto const simulate = [](double const frameSeconds, std::uint64_t const frames)
		{
			HeadlessPlatform platform {HeadlessPlatform::Config {frameSeconds, frames, {640, 480}}};
			NullRenderer renderer;
			CountingApp app;
			GameEngine engine {EngineConfig {}, platform, &renderer};
			engine.start(app);
			platform.run([&]
				{
					return engine.frame(platform.timeSeconds());
				});
			double const x = engine.world().get<TransformComponent>(app.mover).position.x;
			int const ticks = app.ticks;
			engine.shutdown();
			return std::pair {ticks, x};
		};
		// One second of real time rendered at 30, 60 and 144 FPS.
		auto const at30 = simulate(1.0 / 30.0, 31);
		auto const at60 = simulate(1.0 / 60.0, 61);
		auto const at144 = simulate(1.0 / 144.0, 145);
		checkEqual(at30.first, 60, "30 FPS: 60 ticks");
		checkEqual(at60.first, 60, "60 FPS: 60 ticks");
		checkEqual(at144.first, 60, "144 FPS: 60 ticks");
		checkNear(at30.second, 1.0, 1e-9, "30 FPS: same position");
		checkNear(at144.second, at60.second, 1e-12, "144 FPS: identical position");
	}};

TestCase const deterministicStepping {"GameEngine: deterministic stepping without a window or renderer", []
	{
		HeadlessPlatform platform;
		NullRenderer renderer;
		CountingApp app;
		GameEngine engine {EngineConfig {}, platform, &renderer};
		engine.start(app);
		engine.stepSimulation(30);
		checkEqual(app.ticks, 30, "exact tick count");
		checkNear(engine.world().get<TransformComponent>(app.mover).position.x, 0.5, 1e-12, "half a second at 1 m/s");
		checkNear(engine.timing().simulationSeconds, 0.5, 1e-12, "simulation time");
		checkEqual(renderer.statistics().framesRendered, std::uint64_t {0}, "stepping does not render");
		engine.shutdown();
	}};

TestCase const headlessWithoutRenderer {"GameEngine: runs with no renderer at all", []
	{
		class Minimal final : public Application
		{
		public:
			void onFixedUpdate(GameEngine& engine, double) override
			{
				if (++ticks == 10)
				{
					engine.requestQuit();
				}
			}
			int ticks = 0;
		};
		HeadlessPlatform platform;
		Minimal app;
		GameEngine engine {EngineConfig {}, platform, nullptr};
		engine.run(app);
		checkEqual(app.ticks, 10, "quit request honoured");
		checkEqual(platform.framesRun(), std::uint64_t {11}, "first frame has no tick, then one tick per frame");
	}};

TestCase const inputAcrossTicks {"GameEngine: a press is seen by one tick when a frame runs several", []
	{
		// 20 FPS rendering with a 60 Hz simulation: three ticks per frame.
		HeadlessPlatform platform {HeadlessPlatform::Config {1.0 / 20.0, 10, {640, 480}}};
		platform.scheduleKey(3, Key::Space, true);
		platform.scheduleKey(6, Key::Space, false);
		NullRenderer renderer;
		CountingApp app;
		GameEngine engine {EngineConfig {}, platform, &renderer};
		engine.run(app);
		checkEqual(app.fired, 1, "fired exactly once while held across many ticks");
		checkEqual(app.ticks, 27, "nine intervals of three ticks");
	}};

TestCase const resizePropagates {"GameEngine: window resizes reach the renderer", []
	{
		HeadlessPlatform platform {HeadlessPlatform::Config {1.0 / 60.0, 0, {640, 480}}};
		NullRenderer renderer;
		CountingApp app;
		GameEngine engine {EngineConfig {}, platform, &renderer};
		engine.start(app);
		engine.frame(0.0);
		platform.setExtent({1024, 768});
		engine.frame(1.0 / 60.0);
		checkEqual(renderer.width(), 1024u, "resized width");
		platform.setExtent({0, 0});
		engine.frame(2.0 / 60.0);
		checkEqual(renderer.width(), 1024u, "minimised window keeps the last size");
		checkEqual(renderer.resizeCount(), std::uint64_t {2}, "start + one real resize");
		engine.shutdown();
	}};

TestCase const errorsStillShutDown {"GameEngine: exceptions propagate after a clean shutdown", []
	{
		HeadlessPlatform platform {HeadlessPlatform::Config {1.0 / 60.0, 10, {640, 480}}};
		ThrowingApp app;
		GameEngine engine {EngineConfig {}, platform, nullptr};
		checkThrows<std::runtime_error>([&]
			{
				engine.run(app);
			}, "error propagates");
		check(app.shutDown, "onShutdown still ran");
		check(!engine.isRunning(), "engine stopped");
	}};

TestCase const assetPaths {"AssetManager: relative resolution, caching rules and errors", []
	{
		std::filesystem::path const root = std::filesystem::temp_directory_path() / "lge_asset_test";
		std::filesystem::create_directories(root / "text");
		{
			std::ofstream file {root / "text" / "hello.txt"};
			file << "hello assets";
		}
		AssetManager const assets {root};
		check(assets.exists("text/hello.txt"), "exists");
		check(assets.readText("text/hello.txt") == "hello assets", "read text");
		checkEqual(assets.readBinary("text/hello.txt").size(), std::size_t {12}, "read binary");
		check(!assets.exists("text/missing.txt"), "missing");
		checkThrows<AssetError>([&]
			{
				(void)assets.resolve("text/missing.txt");
			}, "missing throws");
		checkThrows<AssetError>([&]
			{
				(void)assets.resolve("../etc/passwd");
			}, "escaping the root throws");
		checkThrows<AssetError>([&]
			{
				(void)assets.resolve((root / "text/hello.txt").string());
			}, "absolute paths throw");
		checkThrows<AssetError>([]
			{
				AssetManager {"/definitely/not/a/directory"};
			}, "bad root throws");
		std::filesystem::remove_all(root);
	}};

}
