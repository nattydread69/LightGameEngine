# Architecture

## Goals

A small engine for modern versions of classic games: a top-down robot action game
(Paradroid-like) and a 3D landscape strategy game (Sentinel-like) are the reference
use cases. Gameplay simulation and graphical presentation are strictly separated, and
everything except the Vulkan adapter runs headlessly.

## Modules and dependency direction

Dependencies point downwards only. Each box is a CMake target (alias in brackets).

```
                 ┌──────────────────────────────────────────┐
 games           │ RobotArenaGame, BasicScene, your game     │
                 └───────────────┬──────────────────────────┘
                                 ▼
                 ┌──────────────────────────────────────────┐
                 │ LightGameEngine [LightGame::Engine]       │  GameEngine, Application,
                 │                                           │  IPlatform, HeadlessPlatform
                 └──────┬───────────────────────────┬───────┘
                        ▼                           ▼
     ┌──────────────────────────────────┐  ┌────────────────────────┐
     │ LightGameScene [LightGame::Scene] │  │ LightGameAI [::AI]      │  A* grid pathfinding,
     │ GameWorld, components, render     │  │                         │  steering
     │ extraction, PhysicsSystem2D       │  └────────────────────────┘
     └───┬──────────────┬───────────┬───┘
         ▼              ▼           ▼
 ┌──────────────┐ ┌───────────┐ ┌──────────────────────────────┐
 │ Core [::Core] │ │ Physics   │ │ Rendering [::Rendering]       │  IRenderer, RenderScene,
 │ math, clock,  │ │ [::Physics]│ │                               │  handles, resource cache
 │ fixed step,   │ │ 2D shapes, │ └───────▲──────────────▲───────┘
 │ ECS, input,   │ │ queries    │         │              │
 │ asset paths   │ └───────────┘  ┌──────┴─────┐ ┌──────┴──────────────────────┐
 └──────────────┘                 │ RendererNull│ │ RendererVulkan               │
                                  │ [::Renderer │ │ [::RendererVulkan]           │
                                  │  Null]      │ │ LightVulkanBackend           │
                                  └────────────┘ └──────┬──────────────────────┘
                                                        ▼ (PRIVATE link)
                                        LightVulkanGraphics 3.2+ + its GLFW
```

| Module | Responsibility |
|---|---|
| **Core** | `Transform` and coordinate conventions, `GameClock`, `FixedTimestep`, the ECS `Registry` (header-only), `InputState`/`ActionMap`, `AssetManager`. Depends only on GLM. |
| **Physics** | 2D AABB/circle overlap, contact and ray tests; `CollisionWorld2D` with layer masks and sort-and-sweep pair finding. Independent of the ECS and rendering. |
| **AI** | `GridMap` + `GridPathfinder` (A*, optional diagonals without corner cutting); stateless steering helpers. No behaviour is hard-coded; games write their own state machines. |
| **Rendering** | The backend-independent `IRenderer`, `RenderScene` snapshot types, `ResourceHandle`/`HandlePool`, `RenderResourceCache`. No GPU code. |
| **Scene** | `GameWorld` (entities, names, active camera, deferred destruction, attachments, velocity integration, render extraction) and `PhysicsSystem2D` (binds 2D collision to entities). |
| **Engine** | `GameEngine` loop, `Application` callbacks, `IPlatform`, `HeadlessPlatform`. |
| **RendererNull** | Headless `IRenderer` that validates and counts submissions. |
| **RendererVulkan** | `LightVulkanBackend`: `IPlatform` + `IRenderer` over LightVulkanGraphics. |

All public headers live under `include/LightGameEngine/<Module>/`; backend headers live
under `backends/<Name>/include`. The layering above is enforced by target link
dependencies, and the `ArchitectureNoVulkanInEngineHeaders` test fails the build's test
run if any engine source or public header outside `backends/Vulkan/src` mentions
Vulkan, GLFW or LightVulkanGraphics.

## Conventions

- **World axes:** right-handed, **+Y up**, **−Z forward**, +X right (GLM/OpenGL
  convention, matching LightVulkanGraphics).
- **Transforms:** `world = T * R * S * local`; column-major GLM matrices acting on
  column vectors. Positions are `glm::dvec3` (double); the renderer converts to float.
- **Yaw:** `yawRotation(θ)` rotates about +Y; θ = 0 faces −Z and positive angles turn
  counter-clockwise seen from above.
- **Cameras** look along their local −Z with local +Y up. Projection matrices from
  `RenderCamera::projectionMatrix()` use OpenGL clip conventions; backends adapt them.
- **Top-down / 2D games** use the XZ ground plane: plane coordinate (u, v) = world
  (x, z) (`Physics/Plane.h`). A camera looking down −Y with up = −Z puts north (−Z) at
  the top of the screen. 2D physics never touches the Y coordinate, so 2D objects pay
  nothing for living in a 3D world.
- **Units:** metres, seconds, radians (degrees only where named, e.g. field of view).

## The frame and the tick

`GameEngine` separates real time from simulated time:

```
frame (variable rate, driven by IPlatform::run):
    clock.beginFrame(now) ─► platform.pollInput ─► resize renderer if needed
    steps = fixedTimestep.advance(frameSeconds)
    repeat steps times: tick()
    application.onFrame()                      // presentation only
    world.extractRenderScene(alpha)            // interpolated snapshot
    application.onBuildRenderScene()           // HUD overlays
    renderer.render(scene)

tick (fixed rate, default 60 Hz):
    input.beginTick()                          // publish pending key edges
    world.storeTransformHistory()              // for interpolation
    application.onFixedUpdate(step)            // gameplay rules
    world.integrateVelocities(step)
    physics.step(world)                        // push-out + collision events
    world.updateAttachments()
    world.flushDestroyQueue()
```

- Frame times above `maxFrameSeconds` (0.25 s) are clamped, and at most
  `maxStepsPerFrame` (8) ticks run per frame. Time beyond that is discarded rather
  than accumulated, so a slow machine slows the game down instead of spiralling.
- Simulation results depend only on the number of ticks, never on the frame rate.
  `EngineTests` and `RobotArenaHeadlessTests` check that 30, 60 and 144 FPS produce
  identical simulations.
- `GameEngine::stepSimulation(n)` runs exactly `n` ticks without a platform or renderer,
  for deterministic headless simulation.

### Input across ticks

The platform reports key levels once per frame. `InputState` turns level changes into
*pending* press/release edges, and `beginTick()` hands them to the next tick and clears
them. Consequently a press is seen by exactly one tick. It is not repeated when a frame
runs several ticks, and it is not lost when a frame runs none. A tap that starts and
ends between two ticks reports both `wasPressed` and `wasReleased`. `isDown` is always
the latest polled level. `ActionMap` maps named actions to any number of keys or mouse
buttons; gameplay queries actions, never keys. The Vulkan backend enables GLFW's
sticky keys so taps shorter than a frame survive polling.

## State separation and ownership

| State | Owner | Lifetime |
|---|---|---|
| Authoritative simulation state (`TransformComponent`, velocities, colliders, game components) | `GameWorld`'s `Registry`, owned by `GameEngine` | Until the entity is destroyed |
| Visual state (`PreviousTransformComponent`, interpolated transforms in `RenderScene`) | `GameWorld` / the per-frame `RenderScene` (reused) | One tick / one frame |
| GPU resources (meshes, materials, per-object instances, text) | The `IRenderer` implementation | Until destroyed through the renderer |
| Resource handles | Whoever created them (often a `RenderResourceCache`) | Plain values; validity is checked by generation |

- The renderer never sees entities. It receives a `RenderScene` value describing *what*
  to draw this frame, with stable `RenderObjectId`s so it can keep per-object GPU state
  between frames.
- Destroying an entity removes it from the next snapshot, and the backend hides or
  recycles its GPU object. No GPU resource is freed while a frame may still use it
  (see [rendering.md](rendering.md#resource-lifetime-and-deferred-destruction)).
- Destroying a mesh or material invalidates its handle immediately. Objects still
  referencing it are skipped and counted in `RenderStatistics::rejectedObjects`, not
  treated as errors.
- `GameEngine` borrows the platform and renderer by reference/pointer. The caller owns
  them and they must outlive the engine. With LightVulkanGraphics one object
  (`LightVulkanBackend`) is both, mirroring the library, which owns the window, the
  event loop and the device together.

## Entity Component System

EnTT was the preferred option, but it is not installed and the build must not assume
Internet access, so the engine has its own small sparse-set ECS (`ECS/Registry.h`).
It provides:

- 32-bit index + 32-bit generation entity ids; stale ids are detected.
- One packed array per component type: O(1) add, remove and lookup; cache-friendly
  iteration; swap-and-pop removal.
- `each<A, B, ...>(fn)` iterates the smallest of the involved pools and probes the
  others, with no virtual calls per entity.
- Components are plain value types (move-only types are supported). Gameplay data
  never inherits from rendering classes.

**Structural rule:** do not create or destroy entities, or add or remove components of
the iterated types, inside `each()`. Use `GameWorld::queueDestroy()` and collect spawn
requests for after the loop (RobotArena shows both). Removing the *current* entity's
components is tolerated, because iteration runs backwards.

Component type ids are assigned on first use from a process-wide counter, so the
registry can index pools by type in a vector. This assumes a single copy of the engine
per process (static libraries, which is how the engine is built).

## Transform hierarchy

There is deliberately only one form of hierarchy: `AttachmentComponent` makes an entity
follow a parent with a local offset, resolved once per tick after physics. RobotArena
uses it for robot domes and visors, and BasicScene for a light following a moving
object. Chains resolve parent-first, `attach()` rejects cycles, and destroying a parent
destroys its attached children at the same flush. There is no general scene graph,
local/world dual storage or reparenting API; add them when a game needs them.

## Physics

`PhysicsSystem2D` rebuilds a `CollisionWorld2D` from `Transform` + `Collider2D` each tick,
finds overlapping pairs (sort-and-sweep on X, filtered by layer masks), pushes dynamic
colliders out of static ones and splits the correction between two dynamic ones, and
records `CollisionEvent`s (including triggers) for gameplay. It is a kinematic
character-collision layer, not rigid-body dynamics. Raycasts and line-of-sight queries
run against the bodies of the last step. Games that need real dynamics should integrate
Box2D (2D) or Jolt (3D) behind the same components.

## Headless operation

`HeadlessPlatform` supplies virtual time, a fixed window size and scripted key events.
Combined with `NullRenderer`, or no renderer at all, the whole engine and RobotArena's
game logic run without a GPU, window or Vulkan installation. The unit tests, the
headless RobotArena integration test and the `GettingStarted` example all run this way,
and `cmake --preset headless` builds without looking for Vulkan at all.

## Design trade-offs

- **Platform-driven loop.** LightVulkanGraphics owns its event loop (`run()` blocks and
  calls an update callback), so `IPlatform::run(frameFunction)` inverts control: the
  engine supplies a per-frame function and the platform calls it. The headless platform
  uses a plain loop, so engine code is identical in both cases.
- **No game concepts in LightVulkanGraphics.** Its 3.2 API covered everything the first
  milestone needed. The three library bugs found during integration were fixed in the
  library itself as generic fixes (3.3), and the adapter keeps version-gated
  workarounds so it still works with 3.2. The library stays general-purpose and
  independently usable.
- **Small custom ECS rather than EnTT.** Chosen to avoid an unavailable dependency. The
  `Registry` API is close enough to EnTT's that switching later would mainly be
  mechanical.
- **No threading yet.** The registry, world and renderer are single-threaded (a
  LightVulkanGraphics requirement as well). The fixed-step design, value-type snapshots
  and lack of global state leave room for job-based systems or parallel extraction later.
- **One shared include tree.** All core modules publish headers under one `include/`
  directory; module boundaries are enforced by link dependencies rather than include
  paths.

## Extending towards the target games

- **Paradroid-style:** grid levels (`GridMap`), A*, line of sight, 2D collision with
  layers, attachments for robot parts, orthographic top-down camera and HUD text all
  exist. Next: sprite/textured-quad rendering through custom meshes with textures,
  per-robot data assets, a state-machine helper if several games want one, audio.
- **Sentinel-style:** perspective cameras, double-precision positions with a moving
  render origin, procedural custom meshes (BasicScene's terrain) and many static
  instanced objects are supported. Next: 3D ray casts against height fields,
  visibility queries, and a terrain module once the game's needs are concrete.
