# Rendering

## The abstraction

The engine describes *what* to draw in engine terms; backends decide *how*.

```cpp
class IRenderer
{
	MeshHandle createMesh(MeshDescription const&);         // primitive shape or custom MeshData
	void destroyMesh(MeshHandle);
	MaterialHandle createMaterial(MaterialDescription const&); // colour, emissive, texture
	void destroyMaterial(MaterialHandle);
	void resize(std::uint32_t width, std::uint32_t height);
	void render(RenderScene const&);                       // one frame
	RenderStatistics statistics() const;
};
```

- **Resources** are created once and referenced by `MeshHandle`/`MaterialHandle`: plain
  index + generation values with no GPU pointers. `HandlePool` provides identical
  stale-handle detection in every backend.
- **`RenderScene`** is a per-frame snapshot: a `RenderCamera`, a `RenderEnvironment`
  (clear colour, ambient light), `RenderObject`s (stable id, mesh, material,
  interpolated world transform in double precision), `RenderLight`s and `ScreenText`
  overlays. It holds no GPU data, and `GameEngine` reuses one instance so its vectors are
  not reallocated every frame.
- **Supported primitives:** cube, sphere, cylinder, cone and capsule, all unit-sized and
  centred, scaled by the transform. **Custom meshes** are indexed triangle lists with
  per-vertex position, normal and colour, validated by `validateMeshData()` in every
  backend.

## How a frame is built

1. `GameWorld::extractRenderScene(scene, alpha)` walks `Transform` + `MeshRenderer`
   entities (skipping invisible ones), interpolating each transform between the
   previous and current tick by `alpha`; adds `Light` entities; and copies the active
   camera.
2. `Application::onBuildRenderScene` may append overlays such as HUD text.
3. `IRenderer::render(scene)` consumes the snapshot.

## LightVulkanBackend

`LightVulkanBackend` (`backends/Vulkan`) implements `IRenderer` and `IPlatform` on top
of LightVulkanGraphics' public API (3.2 or later; 3.3 recommended). It owns one `lightGraphics::lightVulkanGraphics`
instance, which in turn owns the GLFW window, the Vulkan device and the swapchain.
Its header exposes engine types only; LightVulkanGraphics and GLFW are behind a pimpl.

### Frame loop

LightVulkanGraphics' `run()` owns the event loop. `LightVulkanBackend::run()` installs
the engine's frame function as the library's update callback, so each iteration is:
poll events (library) → engine frame (input, ticks, `render(scene)`) → `drawFrame()`
(library). Returning `false` from the frame sets GLFW's close flag. The library's
built-in keyboard and mouse camera controls are disabled; the engine owns the camera.

### Mapping engine concepts to LightVulkanGraphics

| Engine | LightVulkanGraphics | Notes |
|---|---|---|
| Primitive mesh + material | instanced "flexible shape" object (`addObject`, `ShapeType::CUBE/SPHERE/...`) | One library object per `RenderObjectId`, kept across frames. Only changed properties are sent (`updateObjectProperties`, `setObjectColor`, `setObjectEmissive`, `setObjectTexturePath`), so static objects cost nothing after their first frame. |
| Object no longer submitted | `setObjectVisible(false)`, slot recycled | No add/remove churn; indices stay stable. |
| Custom mesh | `createStaticMesh` + `drawMesh` with a `VertexColorLit`/`VertexColorUnlit` material | The material's base colour is baked into the vertex colours, one GPU mesh per (mesh, material) pair, created lazily. Emissive ≥ 0.5 selects the unlit shader. Draw registrations are only rebuilt when the set of custom draws changes, because each `drawMesh` re-sorts the library's draw list. |
| `RenderCamera` | `setCameraLookAt`, `setCameraProjection`, `setCameraOrthoHeight`, `setCameraFov`, `setCameraPlanes` | Camera roll is not representable. |
| `RenderLight` | `clearLights` + `addLight` | Only re-sent when the light list changes. At most `MaxForwardLights` (16). |
| `RenderEnvironment` | `setClearColor`, `setAmbientLight` | Only sent on change. |
| `ScreenText` | `createScreenText`, `updateScreenText`, `setScreenTextVisible` | Pooled per text id; capacity grows by recreating. Positions are in swapchain pixels from the top-left. |
| Material texture | `setObjectTexturePath` (library caches textures by path) | Primitives only. Paths come from `AssetManager::resolve`. |

### Precision: render origin

World positions are doubles; LightVulkanGraphics uses floats. The backend subtracts a
*render origin* (the camera position snapped to a 1024 m grid) from every position before
converting. Precision therefore stays high far from the world origin, while static
objects only need re-uploading when the camera crosses a grid boundary.

### Resource lifetime and deferred destruction

LightVulkanGraphics keeps up to two frames in flight and frees custom-mesh buffers
immediately in `destroyMesh`. When an engine mesh or material is destroyed, the backend:

1. invalidates the engine handle at once (later submissions referencing it are rejected);
2. removes the affected baked GPU meshes from the draw list on the next `render()`;
3. queues them for release and calls `destroyMesh` only once `frames-in-flight + 1`
   frames have been rendered, after the library has waited on the fence of the last
   frame that could reference them.

Instanced primitive objects are never destroyed during play (they are hidden and
recycled), and screen texts are hidden rather than destroyed, so neither can be freed
under the GPU. Everything else is released by LightVulkanGraphics' own `cleanup()` when
the backend is destroyed. No path calls `vkDeviceWaitIdle` per frame.

### Workarounds for LightVulkanGraphics 3.2

Three issues found during integration are fixed in LightVulkanGraphics 3.3.0 (see its
CHANGELOG). The adapter checks the
library version at compile time (`LIGHT_VULKAN_GRAPHICS_VERSION_*`) and applies the
workarounds below only when built against 3.2:

1. **Zero-vertex draw with no instanced objects.** If a frame has custom-mesh draws
   (screen text counts) but no flexible-shape objects, `recordCommandBuffer()` reaches a
   fallback `vkCmdDraw(cmd, 0, …)` with no vertex buffer bound, which crashed the Intel
   driver here. The backend keeps one permanently hidden object so the library always
   takes its normal path.
2. **Depth range.** The library's projections use GLM's OpenGL depth range (−1…1) while
   Vulkan clips to 0…1, so only the far half of the depth range is visible. For
   orthographic cameras that hid everything nearer than (near + far) / 2. The backend
   passes adjusted planes so the requested near…far range lands exactly in the visible
   half: n′ = 2n − f for orthographic, n′ = nf / (2f − n) for perspective. (3.2 also
   lost shadows in the nearer half of each light's shadow volume; the adapter cannot
   correct that.)
3. **Swapchain size.** 3.2 has no swapchain-size query, and on Wayland its swapchain
   keeps the window's *initial* logical size even though GLFW reports a larger HiDPI
   framebuffer. The backend reports the window size, which matches on X11, Windows and
   Wayland until a resize.

With 3.3 the backend uses plain near/far planes, adds no placeholder object, and
reports `VkApp::getSwapchainExtent()`. On Wayland HiDPI displays this means rendering
at native resolution (2880 × 1800 instead of 1440 × 900 on the development machine).

## NullRenderer

`NullRenderer` implements the same interface without any graphics API. It stores
resource descriptions in the same `HandlePool`s, validates custom mesh data, counts
submitted, drawn and rejected objects, lights and texts per frame, and keeps a copy of
the last scene for assertions. It is not a software rasteriser and does not simulate
GPU behaviour.

## CPU assets to GPU resources

```
assets/textures/arena_floor.png       source asset (relative path)
        │ AssetManager::resolve()      → absolute path, existence checked, no ".."
        ▼
MaterialDescription::texture          CPU-side description
        │ RenderResourceCache::material("arena/floor", …)   created once per key
        ▼
MaterialHandle                        engine identifier, shared by many entities
        │ LightVulkanBackend: setObjectTexturePath()
        ▼
LightVulkanGraphics texture cache     GPU image + descriptor, cached by path
```

`RenderResourceCache` owns the handles it creates and releases them through the
renderer in `release*()` or its destructor. Invalidation is explicit: there is no
reference counting or hot reload.
