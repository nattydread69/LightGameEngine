# Getting started

This walks through a complete minimal game: a ball that falls, bounces and jumps when
Space is pressed. The code is the `GettingStarted` example
(`examples/GettingStarted/main.cpp`), which is built and run as part of the test suite,
so the code shown here is known to compile.

## 1. Build the engine

```bash
cmake -S . -B build -G Ninja
cmake --build build
./build/examples/GettingStarted/GettingStarted
# The ball bounced 7 times
# Simulated 9.98333 s in 599 ticks; rendered 600 frames
```

## 2. Write an Application

Game behaviour lives in a class derived from `lge::Application`. The engine calls it;
it never calls the engine's loop itself.

```cpp
#include "LightGameEngine/Backends/NullRenderer.h"
#include "LightGameEngine/Engine/GameEngine.h"
#include "LightGameEngine/Platform/HeadlessPlatform.h"

class BouncingBall final : public lge::Application
{
public:
	void onStart(lge::GameEngine& engine) override
	{
		// Renderer resources are created once and shared by handle.
		lge::IRenderer& renderer = *engine.renderer();
		mesh_ = renderer.createMesh(lge::MeshDescription::primitive(lge::PrimitiveShape::Sphere));
		material_ = renderer.createMaterial(lge::MaterialDescription {{1.0f, 0.4f, 0.2f, 1.0f}});

		// Entities are ids; data lives in components.
		lge::GameWorld& world = engine.world();
		ball_ = world.createEntity("Ball", lge::Transform {{0.0, 5.0, 0.0}});
		world.add<lge::MeshRendererComponent>(ball_, lge::MeshRendererComponent {mesh_, material_});
		world.add<lge::VelocityComponent>(ball_);

		lge::Entity const camera = world.createEntity("Camera", lge::Transform {{0.0, 3.0, 12.0}});
		world.add<lge::CameraComponent>(camera);
		world.setActiveCamera(camera);

		// Gameplay asks about actions, not keys.
		jump_ = engine.actions().bind("Jump", lge::Key::Space);
	}

	// Runs at a fixed 60 Hz regardless of the frame rate.
	void onFixedUpdate(lge::GameEngine& engine, double const step) override
	{
		lge::VelocityComponent& velocity = engine.world().get<lge::VelocityComponent>(ball_);
		lge::TransformComponent& transform = engine.world().get<lge::TransformComponent>(ball_);
		velocity.linear.y -= 9.81 * step;
		if (transform.position.y < 0.5 && velocity.linear.y < 0.0)
		{
			transform.position.y = 0.5;
			velocity.linear.y = -velocity.linear.y * 0.8;
			++bounces_;
		}
		if (engine.actions().wasTriggered(jump_, engine.input()))
		{
			velocity.linear.y = 8.0;
		}
	}

	void onShutdown(lge::GameEngine& engine) override
	{
		engine.renderer()->destroyMesh(mesh_);
		engine.renderer()->destroyMaterial(material_);
	}

private:
	lge::MeshHandle mesh_;
	lge::MaterialHandle material_;
	lge::Entity ball_;
	lge::ActionId jump_;
	int bounces_ = 0;
};
```

The engine integrates `VelocityComponent` into the transform after `onFixedUpdate`,
then runs 2D collision for entities with colliders, so the game only sets velocities.

## 3. Run it headlessly

```cpp
int main()
{
	lge::HeadlessPlatform platform {lge::HeadlessPlatform::Config {1.0 / 60.0, 600}};
	platform.scheduleKey(300, lge::Key::Space, true);   // scripted input at frame 300
	lge::NullRenderer renderer;

	lge::GameEngine engine {lge::EngineConfig {"GettingStarted"}, platform, &renderer};
	BouncingBall game;
	engine.run(game);
}
```

This needs no window, GPU or Vulkan, and time is virtual, so every run is identical. It
is the basis for testing game logic.

## 4. Put it on screen

Swap the platform and renderer for the LightVulkanGraphics backend, which plays both
roles because the library owns the window:

```cpp
#include "LightGameEngine/Backends/LightVulkanBackend.h"

int main()
{
	lge::LightVulkanBackend backend {lge::LightVulkanBackendConfig {"Bouncing Ball", 1280, 720}};
	lge::GameEngine engine {lge::EngineConfig {"BouncingBall"}, backend, &backend};
	BouncingBall game;
	engine.run(game);   // returns when the window is closed or engine.requestQuit() is called
}
```

Link `LightGame::RendererVulkan` in addition to `LightGame::Engine`. The game code itself
does not change.

## 5. Use the engine from your own repository

```cmake
cmake_minimum_required(VERSION 3.20)
project(MyGame CXX)

add_subdirectory(path/to/LightGameEngine lightGameEngine)   # examples/tests stay off
add_executable(MyGame main.cpp)
target_link_libraries(MyGame PRIVATE LightGame::Engine LightGame::RendererVulkan)
```

## Where to go next

- `examples/RobotArena` shows a complete game: walls with static colliders, robots
  with attached parts, projectiles as triggers, A* patrols, line-of-sight chasing,
  deferred spawning and destruction, HUD text, and a restartable round.
- `examples/BasicScene` shows custom procedural meshes, lights and switching between
  perspective and orthographic cameras.
- [architecture.md](architecture.md) explains the loop, ownership and ECS rules;
  [rendering.md](rendering.md) explains the renderer abstraction and the Vulkan adapter.

### Rules worth knowing early

- Change gameplay state only in `onFixedUpdate`. Use `onFrame` for presentation
  (camera smoothing, HUD).
- Do not create or destroy entities inside `world.each<...>()`. Queue spawns and use
  `world.queueDestroy()`.
- Collision events in `engine.physics().events()` come from the previous tick; check
  `world.isAlive()` before using their entities.
- Move an entity with `world.teleport()` when it should not visually slide from its old
  position.
