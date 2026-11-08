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

#include "LightGameEngine/Backends/LightVulkanBackend.h"

#include "LightGameEngine/Input/InputState.h"
#include "LightGameEngine/Rendering/ResourceHandle.h"

#include "GlfwKeyMap.h"

#include <lightVulkanGraphics.h>

// LightVulkanGraphics 3.3 fixed three issues that this adapter works around on 3.2:
// a crash when no instanced objects exist, the clipped near half of the depth range, and
// the missing swapchain-size query. The preprocessor is needed because the 3.3-only
// accessor does not compile against 3.2 headers.
#if LIGHT_VULKAN_GRAPHICS_VERSION_MAJOR > 3 || \
	(LIGHT_VULKAN_GRAPHICS_VERSION_MAJOR == 3 && LIGHT_VULKAN_GRAPHICS_VERSION_MINOR >= 3)
#define LGE_LIGHT_VULKAN_GRAPHICS_3_3 1
#else
#define LGE_LIGHT_VULKAN_GRAPHICS_3_3 0
#endif

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>
#include <cmath>
#include <iostream>
#include <map>
#include <optional>
#include <stdexcept>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace lge
{

namespace
{

namespace lg = lightGraphics;

// LightVulkanGraphics renders in single precision. Positions are sent relative to a
// render origin that follows the camera in coarse steps, so precision stays high far
// from the world origin while static objects only need re-uploading when it moves.
constexpr double renderOriginSnap = 1024.0;

// Frames LightVulkanGraphics may have in flight (VkApp::MAX_FRAMES_IN_FLIGHT). A GPU
// buffer last referenced while recording frame N is safe to free once the frame
// N + framesInFlight has waited on N's fence, i.e. from frame N + framesInFlight + 1.
constexpr std::uint64_t framesInFlight = 2;

constexpr bool libraryNeedsWorkarounds = LGE_LIGHT_VULKAN_GRAPHICS_3_3 == 0;

lg::ShapeType toShape(PrimitiveShape const shape)
{
	switch (shape)
	{
	case PrimitiveShape::Cube:
		return lg::ShapeType::CUBE;
	case PrimitiveShape::Sphere:
		return lg::ShapeType::SPHERE;
	case PrimitiveShape::Cylinder:
		return lg::ShapeType::CYLINDER;
	case PrimitiveShape::Cone:
		return lg::ShapeType::CONE;
	case PrimitiveShape::Capsule:
		return lg::ShapeType::CAPSULE;
	}
	return lg::ShapeType::CUBE;
}

lg::LightType toLightType(LightType const type)
{
	switch (type)
	{
	case LightType::Directional:
		return lg::LightType::Directional;
	case LightType::Point:
		return lg::LightType::Point;
	case LightType::Spot:
		return lg::LightType::Spot;
	}
	return lg::LightType::Directional;
}

struct MaterialRecord
{
	MaterialDescription description;
	std::string texturePath;
};

// One LightVulkanGraphics instanced object, reused across frames for the same
// RenderObjectId and recycled (hidden) when that id disappears.
struct ObjectSlot
{
	lg::ObjectHandle handle;
	RenderObjectId id = 0;
	bool inUse = false;
	std::uint64_t lastSeenFrame = 0;
	// Last values sent, so unchanged objects cost no library calls.
	lg::ShapeType shape = lg::ShapeType::CUBE;
	glm::vec3 position {0.0f};
	glm::quat rotation {1.0f, 0.0f, 0.0f, 0.0f};
	glm::vec3 scale {1.0f};
	glm::vec4 color {1.0f};
	float emissive = 0.0f;
	std::string texture;
	glm::vec2 tiling {1.0f};
};

struct TextSlot
{
	lg::ScreenTextHandle handle;
	RenderObjectId id = 0;
	bool inUse = false;
	std::uint64_t lastSeenFrame = 0;
	std::size_t capacity = 0;
	ScreenText last;
};

struct CustomDraw
{
	lg::MeshHandle mesh;
	lg::MaterialHandle material;
	glm::vec3 position;
	glm::quat rotation;
	glm::vec3 scale;

	bool operator==(CustomDraw const& other) const
	{
		return mesh.index == other.mesh.index && mesh.generation == other.mesh.generation &&
			material.index == other.material.index && material.generation == other.material.generation &&
			position == other.position && rotation == other.rotation && scale == other.scale;
	}
};

// Custom meshes are uploaded per (mesh, material) pair because LightVulkanGraphics'
// custom-mesh pipeline colours vertices without a per-draw tint; the material's base
// colour is baked into the vertex colours.
using BakeKey = std::tuple<std::uint32_t, std::uint32_t, std::uint32_t, std::uint32_t>;

struct PendingMeshRelease
{
	lg::MeshHandle mesh;
	std::uint64_t releaseFrame;
};

}

struct LightVulkanBackend::Impl
{
	std::unique_ptr<lg::lightVulkanGraphics> graphics;
	GLFWwindow* window = nullptr;

	HandlePool<MeshTag, MeshDescription> meshes;
	HandlePool<MaterialTag, MaterialRecord> materials;

	std::vector<ObjectSlot> objectSlots;
	std::vector<std::uint32_t> freeObjectSlots;
	std::unordered_map<RenderObjectId, std::uint32_t> objectSlotById;

	std::vector<TextSlot> textSlots;
	std::unordered_map<RenderObjectId, std::uint32_t> textSlotById;

	std::map<BakeKey, lg::MeshHandle> bakedMeshes;
	std::optional<lg::MaterialHandle> litMaterial;
	std::optional<lg::MaterialHandle> unlitMaterial;
	std::vector<CustomDraw> customDraws;
	std::vector<CustomDraw> submittedCustomDraws;
	bool customDrawsDirty = false;
	std::vector<PendingMeshRelease> pendingReleases;

	std::vector<RenderLight> submittedLights;
	glm::dvec3 submittedLightOrigin {0.0};
	bool lightsInitialised = false;
	std::optional<RenderEnvironment> submittedEnvironment;

	glm::dvec3 renderOrigin {0.0};
	std::uint64_t frameNumber = 0;
	RenderStatistics statistics;
	std::uint32_t screenshotsWritten = 0;

	glm::vec3 relative(glm::dvec3 const& world) const
	{
		return glm::vec3 {world - renderOrigin};
	}

	std::size_t objectIndex(ObjectSlot const& slot) const
	{
		return graphics->resolveObjectHandle(slot.handle);
	}

	void updateRenderOrigin(glm::dvec3 const& cameraPosition)
	{
		glm::dvec3 const snapped = glm::floor(cameraPosition / renderOriginSnap) * renderOriginSnap;
		renderOrigin = snapped;
	}

	void applyEnvironment(RenderEnvironment const& environment)
	{
		if (submittedEnvironment && submittedEnvironment->clearColor == environment.clearColor &&
			submittedEnvironment->ambientLight == environment.ambientLight)
		{
			return;
		}
		graphics->setClearColor(glm::vec4 {environment.clearColor, 1.0f});
		graphics->setAmbientLight(environment.ambientLight);
		submittedEnvironment = environment;
	}

	void applyCamera(RenderCamera const& camera)
	{
		glm::vec3 const eye = relative(camera.position);
		glm::vec3 const forward = glm::vec3 {camera.forward()};
		glm::vec3 const up = glm::vec3 {camera.up()};
		// LightVulkanGraphics derives yaw and pitch from the view direction; `up` only
		// orients near-vertical views, so camera roll is not representable.
		graphics->setCameraLookAt(eye, eye + forward, up);

		double nearPlane = camera.nearPlane;
		double const farPlane = camera.farPlane;
		bool const orthographic = camera.projection == ProjectionType::Orthographic;
		if (libraryNeedsWorkarounds)
		{
			// LightVulkanGraphics 3.2 projections use GLM's OpenGL depth range (-1..1)
			// while Vulkan clips depth to 0..1, so only the far half of the depth range
			// is visible. Pass planes that put the requested near..far range exactly in
			// that visible half:
			//   orthographic (linear):  z = 0 at the midpoint of [n', f]  =>  n' = 2n - f
			//   perspective:            z = 0 at d = 2fn'/(f + n')       =>  n' = nf / (2f - n)
			nearPlane = orthographic ? 2.0 * nearPlane - farPlane : nearPlane * farPlane / (2.0 * farPlane - nearPlane);
		}
		if (orthographic)
		{
			graphics->setCameraProjection(Camera::Projection::Orthographic);
			graphics->setCameraOrthoHeight(static_cast<float>(std::max(camera.orthographicHeight, 1e-3)));
		}
		else
		{
			graphics->setCameraProjection(Camera::Projection::Perspective);
			graphics->setCameraFov(static_cast<float>(camera.verticalFieldOfViewDegrees));
		}
		graphics->setCameraPlanes(static_cast<float>(nearPlane), static_cast<float>(farPlane));
	}

	ObjectSlot& acquireObjectSlot(RenderObjectId const id, lg::ShapeType const shape)
	{
		if (auto const found = objectSlotById.find(id); found != objectSlotById.end())
		{
			return objectSlots[found->second];
		}
		std::uint32_t index = 0;
		if (!freeObjectSlots.empty())
		{
			index = freeObjectSlots.back();
			freeObjectSlots.pop_back();
		}
		else
		{
			lg::ObjectDescription description;
			description.type = shape;
			description.size = glm::vec3 {1.0f};
			ObjectSlot slot;
			slot.handle = graphics->addObject(description);
			slot.shape = shape;
			index = static_cast<std::uint32_t>(objectSlots.size());
			objectSlots.push_back(slot);
		}
		ObjectSlot& slot = objectSlots[index];
		slot.id = id;
		slot.inUse = true;
		objectSlotById.emplace(id, index);
		graphics->setObjectVisible(objectIndex(slot), true);
		return slot;
	}

	void syncPrimitive(RenderObject const& object, PrimitiveShape const shape, MaterialRecord const& material)
	{
		lg::ShapeType const lgShape = toShape(shape);
		ObjectSlot& slot = acquireObjectSlot(object.id, lgShape);
		slot.lastSeenFrame = frameNumber;
		std::size_t const index = objectIndex(slot);

		glm::vec3 const position = relative(object.position);
		glm::quat const rotation = glm::quat {object.rotation};
		glm::vec3 const scale = glm::vec3 {object.scale};
		MaterialDescription const& description = material.description;

		if (slot.shape != lgShape)
		{
			// A recycled slot may hold another shape; rewrite the whole description.
			lg::ObjectDescription full;
			full.type = lgShape;
			full.position = position;
			full.size = scale;
			full.rotation = rotation;
			full.color = description.baseColor;
			full.texturePath = material.texturePath;
			full.textureTiling = description.textureTiling;
			graphics->updateObject(index, full);
			graphics->setObjectEmissive(index, description.emissive);
			slot.shape = lgShape;
			slot.position = position;
			slot.rotation = rotation;
			slot.scale = scale;
			slot.color = description.baseColor;
			slot.emissive = description.emissive;
			slot.texture = material.texturePath;
			slot.tiling = description.textureTiling;
			return;
		}
		if (slot.position != position || slot.rotation != rotation || slot.scale != scale)
		{
			graphics->updateObjectProperties(index, position, scale, rotation);
			slot.position = position;
			slot.rotation = rotation;
			slot.scale = scale;
		}
		if (slot.color != description.baseColor)
		{
			graphics->setObjectColor(index, description.baseColor);
			slot.color = description.baseColor;
		}
		if (slot.emissive != description.emissive)
		{
			graphics->setObjectEmissive(index, description.emissive);
			slot.emissive = description.emissive;
		}
		if (slot.texture != material.texturePath)
		{
			graphics->setObjectTexturePath(index, material.texturePath);
			slot.texture = material.texturePath;
		}
		if (slot.tiling != description.textureTiling)
		{
			graphics->setObjectTextureTiling(index, description.textureTiling);
			slot.tiling = description.textureTiling;
		}
	}

	void hideUnseenObjects()
	{
		for (std::uint32_t i = 0; i < objectSlots.size(); ++i)
		{
			ObjectSlot& slot = objectSlots[i];
			if (slot.inUse && slot.lastSeenFrame != frameNumber)
			{
				graphics->setObjectVisible(objectIndex(slot), false);
				objectSlotById.erase(slot.id);
				slot.inUse = false;
				freeObjectSlots.push_back(i);
			}
		}
	}

	lg::MaterialHandle customMaterial(bool const unlit)
	{
		std::optional<lg::MaterialHandle>& cached = unlit ? unlitMaterial : litMaterial;
		if (!cached)
		{
			lg::MaterialDescription description;
			description.shaderProgram.selection = unlit ? lg::ShaderSelection::VertexColorUnlit : lg::ShaderSelection::VertexColorLit;
			cached = graphics->createMaterial(description);
		}
		return *cached;
	}

	lg::MeshHandle bakedMesh(MeshHandle const mesh, MeshData const& data, MaterialHandle const material,
		MaterialDescription const& description)
	{
		BakeKey const key {mesh.index, mesh.generation, material.index, material.generation};
		if (auto const found = bakedMeshes.find(key); found != bakedMeshes.end())
		{
			return found->second;
		}
		lg::MeshData baked;
		baked.vertices.reserve(data.vertices.size());
		for (MeshVertex const& vertex : data.vertices)
		{
			lg::MeshVertex out;
			out.position = vertex.position;
			out.normal = vertex.normal;
			out.color = vertex.color * description.baseColor;
			baked.vertices.push_back(out);
		}
		baked.indices = data.indices;
		lg::MeshHandle const handle = graphics->createStaticMesh(baked);
		bakedMeshes.emplace(key, handle);
		return handle;
	}

	// Schedules GPU release of every baked mesh built from the given engine resource.
	template <typename Predicate>
	void retireBakedMeshes(Predicate&& matches)
	{
		for (auto it = bakedMeshes.begin(); it != bakedMeshes.end();)
		{
			if (matches(it->first))
			{
				pendingReleases.push_back(PendingMeshRelease {it->second, frameNumber + framesInFlight + 1});
				it = bakedMeshes.erase(it);
				customDrawsDirty = true;
			}
			else
			{
				++it;
			}
		}
	}

	void releaseRetiredMeshes()
	{
		auto const ready = [this](PendingMeshRelease const& pending)
		{
			return pending.releaseFrame <= frameNumber;
		};
		for (PendingMeshRelease const& pending : pendingReleases)
		{
			if (ready(pending))
			{
				graphics->destroyMesh(pending.mesh);
			}
		}
		pendingReleases.erase(std::remove_if(pendingReleases.begin(), pendingReleases.end(), ready), pendingReleases.end());
	}

	void submitCustomDraws()
	{
		// Re-registering draws re-sorts the library's draw list, so only do it when the
		// set of custom draws actually changed.
		if (!customDrawsDirty && customDraws == submittedCustomDraws)
		{
			return;
		}
		graphics->clearMeshDraws();
		for (CustomDraw const& draw : customDraws)
		{
			lg::Transform transform;
			transform.position = draw.position;
			transform.rotation = draw.rotation;
			transform.scale = draw.scale;
			graphics->drawMesh(draw.mesh, transform, draw.material);
		}
		submittedCustomDraws = customDraws;
		customDrawsDirty = false;
	}

	void applyLights(std::vector<RenderLight> const& lights)
	{
		if (lightsInitialised && lights == submittedLights && renderOrigin == submittedLightOrigin)
		{
			return;
		}
		graphics->clearLights();
		std::size_t const count = std::min(lights.size(), lg::MaxForwardLights);
		for (std::size_t i = 0; i < count; ++i)
		{
			RenderLight const& light = lights[i];
			lg::LightSource source;
			source.type = toLightType(light.type);
			source.position = relative(light.position);
			source.direction = light.direction;
			source.color = light.color;
			source.intensity = light.intensity;
			source.range = light.range;
			source.innerConeAngleRadians = light.innerConeRadians;
			source.outerConeAngleRadians = light.outerConeRadians;
			source.castsShadow = light.castsShadow;
			graphics->addLight(source);
		}
		submittedLights = lights;
		submittedLightOrigin = renderOrigin;
		lightsInitialised = true;
	}

	void applyTexts(std::vector<ScreenText> const& texts)
	{
		for (ScreenText const& text : texts)
		{
			auto const found = textSlotById.find(text.id);
			std::size_t const needed = std::max<std::size_t>(text.text.size(), 1);
			if (found != textSlotById.end() && textSlots[found->second].capacity >= needed)
			{
				TextSlot& slot = textSlots[found->second];
				slot.lastSeenFrame = frameNumber;
				if (slot.last.text != text.text || slot.last.positionPixels != text.positionPixels ||
					slot.last.scale != text.scale || slot.last.color != text.color)
				{
					graphics->updateScreenText(slot.handle, describe(text, slot.capacity));
					slot.last = text;
				}
				continue;
			}
			if (found != textSlotById.end())
			{
				// Capacity is fixed at creation; retire the old slot and make a larger one.
				TextSlot& old = textSlots[found->second];
				graphics->setScreenTextVisible(old.handle, false);
				old.inUse = false;
				textSlotById.erase(found);
			}
			TextSlot slot;
			slot.id = text.id;
			slot.inUse = true;
			slot.lastSeenFrame = frameNumber;
			slot.capacity = std::max<std::size_t>(needed * 2, 64);
			slot.handle = graphics->createScreenText(describe(text, slot.capacity));
			slot.last = text;
			textSlotById[text.id] = static_cast<std::uint32_t>(textSlots.size());
			textSlots.push_back(slot);
		}
		for (TextSlot& slot : textSlots)
		{
			if (slot.inUse && slot.lastSeenFrame != frameNumber)
			{
				// Hidden rather than destroyed: the GPU may still be drawing it.
				graphics->setScreenTextVisible(slot.handle, false);
				textSlotById.erase(slot.id);
				slot.inUse = false;
			}
		}
	}

	static lg::ScreenTextDescription describe(ScreenText const& text, std::size_t const capacity)
	{
		lg::ScreenTextDescription description;
		description.text = text.text;
		description.positionPixels = text.positionPixels;
		description.scale = text.scale;
		description.color = text.color;
		description.maximumCharacters = capacity;
		description.visible = true;
		return description;
	}
};

LightVulkanBackend::LightVulkanBackend(LightVulkanBackendConfig const& config)
	: impl_(std::make_unique<Impl>())
{
	lg::LightVulkanGraphicsCreateInfo createInfo;
	createInfo.width = static_cast<int>(config.width);
	createInfo.height = static_cast<int>(config.height);
	createInfo.shaderPath = config.shaderDirectory.string();
	createInfo.enableConsoleOutput = config.graphicsConsoleOutput;
	impl_->graphics = std::make_unique<lg::lightVulkanGraphics>(config.title, createInfo);

	lg::lightVulkanGraphics& graphics = *impl_->graphics;
	// The engine owns camera and input; turn off the library's built-in controls.
	graphics.setKeyboardCameraEnabled(false);
	graphics.setMouseCameraEnabled(false);

	if (libraryNeedsWorkarounds)
	{
		// LightVulkanGraphics 3.2: when a frame has custom-mesh draws (screen text is
		// one) but no instanced objects, recordCommandBuffer() falls through to a
		// zero-vertex vkCmdDraw without bound vertex buffers, which crashes some
		// drivers. A permanently hidden object keeps the library on its instanced path.
		lg::ObjectDescription placeholder;
		placeholder.type = lg::ShapeType::CUBE;
		placeholder.size = glm::vec3 {0.0f};
		graphics.setObjectVisible(graphics.resolveObjectHandle(graphics.addObject(placeholder)), false);
	}

	graphics.finalizeScene();

	impl_->window = graphics.getWindowPointer();
	if (impl_->window == nullptr)
	{
		throw std::runtime_error("LightVulkanBackend: LightVulkanGraphics did not create a window");
	}
	// Sticky input keeps a press visible until it has been polled, so a tap shorter
	// than one frame is still reported.
	glfwSetInputMode(impl_->window, GLFW_STICKY_KEYS, GLFW_TRUE);
	glfwSetInputMode(impl_->window, GLFW_STICKY_MOUSE_BUTTONS, GLFW_TRUE);
}

LightVulkanBackend::~LightVulkanBackend() = default;

void LightVulkanBackend::run(FrameFunction const& frame)
{
	GLFWwindow* const window = impl_->window;
	// LightVulkanGraphics owns the event loop: the engine frame runs inside its
	// per-frame update callback, immediately before the library draws.
	impl_->graphics->setUpdateCallback([&frame, window](float)
		{
			if (!frame())
			{
				glfwSetWindowShouldClose(window, GLFW_TRUE);
			}
		});
	struct ResetCallback
	{
		lg::lightVulkanGraphics& graphics;
		~ResetCallback()
		{
			graphics.setUpdateCallback(nullptr);
		}
	} const reset {*impl_->graphics};
	glfwSetWindowShouldClose(window, GLFW_FALSE);
	impl_->graphics->run();
}

double LightVulkanBackend::timeSeconds() const
{
	return glfwGetTime();
}

void LightVulkanBackend::pollInput(InputState& input)
{
	GLFWwindow* const window = impl_->window;
	for (KeyMapping const& mapping : glfwKeyMap())
	{
		input.setKeyDown(mapping.key, glfwGetKey(window, mapping.glfwKey) == GLFW_PRESS);
	}
	input.setMouseButtonDown(MouseButton::Left, glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS);
	input.setMouseButtonDown(MouseButton::Right, glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_RIGHT) == GLFW_PRESS);
	input.setMouseButtonDown(MouseButton::Middle, glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_MIDDLE) == GLFW_PRESS);
	double x = 0.0;
	double y = 0.0;
	glfwGetCursorPos(window, &x, &y);
	input.setMousePosition(glm::dvec2 {x, y});
}

WindowExtent LightVulkanBackend::framebufferExtent() const
{
#if LGE_LIGHT_VULKAN_GRAPHICS_3_3
	// The swapchain size is the pixel space screen text and the projection use.
	glm::uvec2 const swapchain = impl_->graphics->getSwapchainExtent();
	if (swapchain.x > 0 && swapchain.y > 0)
	{
		return WindowExtent {swapchain.x, swapchain.y};
	}
#endif
	// LightVulkanGraphics 3.2 has no swapchain-size query. Its swapchain matches the
	// window size in screen coordinates on X11 and Windows, and on Wayland (where the
	// surface leaves the extent to the application) it keeps the window's initial size,
	// so the window size is the closest observable value. On macOS Retina displays
	// this underestimates by the content scale.
	int width = 0;
	int height = 0;
	glfwGetWindowSize(impl_->window, &width, &height);
	return WindowExtent {static_cast<std::uint32_t>(std::max(width, 0)), static_cast<std::uint32_t>(std::max(height, 0))};
}

void LightVulkanBackend::requestClose()
{
	glfwSetWindowShouldClose(impl_->window, GLFW_TRUE);
}

MeshHandle LightVulkanBackend::createMesh(MeshDescription const& description)
{
	if (MeshData const* const data = std::get_if<MeshData>(&description.geometry))
	{
		validateMeshData(*data);
	}
	return impl_->meshes.insert(description);
}

void LightVulkanBackend::destroyMesh(MeshHandle const mesh)
{
	if (!impl_->meshes.erase(mesh))
	{
		throw std::invalid_argument("LightVulkanBackend::destroyMesh: handle is invalid or already destroyed");
	}
	impl_->retireBakedMeshes([mesh](BakeKey const& key)
		{
			return std::get<0>(key) == mesh.index && std::get<1>(key) == mesh.generation;
		});
}

MaterialHandle LightVulkanBackend::createMaterial(MaterialDescription const& description)
{
	MaterialRecord record {description, description.texture.empty() ? std::string {} : description.texture.string()};
	return impl_->materials.insert(std::move(record));
}

void LightVulkanBackend::destroyMaterial(MaterialHandle const material)
{
	if (!impl_->materials.erase(material))
	{
		throw std::invalid_argument("LightVulkanBackend::destroyMaterial: handle is invalid or already destroyed");
	}
	impl_->retireBakedMeshes([material](BakeKey const& key)
		{
			return std::get<2>(key) == material.index && std::get<3>(key) == material.generation;
		});
}

void LightVulkanBackend::resize(std::uint32_t, std::uint32_t)
{
	// LightVulkanGraphics observes framebuffer resizes itself and rebuilds its
	// swapchain; the projection aspect follows automatically. Nothing to forward.
}

void LightVulkanBackend::render(RenderScene const& scene)
{
	Impl& impl = *impl_;
	++impl.frameNumber;
	impl.releaseRetiredMeshes();

	impl.updateRenderOrigin(scene.camera.position);
	impl.applyEnvironment(scene.environment);
	impl.applyCamera(scene.camera);

	impl.customDraws.clear();
	std::size_t drawn = 0;
	for (RenderObject const& object : scene.objects)
	{
		MeshDescription const* const mesh = impl.meshes.get(object.mesh);
		MaterialRecord const* const material = impl.materials.get(object.material);
		if (mesh == nullptr || material == nullptr)
		{
			continue;
		}
		if (PrimitiveShape const* const shape = std::get_if<PrimitiveShape>(&mesh->geometry))
		{
			impl.syncPrimitive(object, *shape, *material);
		}
		else
		{
			MeshData const& data = std::get<MeshData>(mesh->geometry);
			impl.customDraws.push_back(CustomDraw {
				impl.bakedMesh(object.mesh, data, object.material, material->description),
				impl.customMaterial(material->description.emissive >= 0.5f),
				impl.relative(object.position),
				glm::quat {object.rotation},
				glm::vec3 {object.scale}});
		}
		++drawn;
	}
	impl.hideUnseenObjects();
	impl.submitCustomDraws();
	impl.applyLights(scene.lights);
	impl.applyTexts(scene.texts);

	impl.statistics.framesRendered = impl.frameNumber;
	impl.statistics.submittedObjects = scene.objects.size();
	impl.statistics.drawnObjects = drawn;
	impl.statistics.rejectedObjects = scene.objects.size() - drawn;
	impl.statistics.lights = std::min(scene.lights.size(), lg::MaxForwardLights);
	impl.statistics.texts = scene.texts.size();
}

RenderStatistics LightVulkanBackend::statistics() const
{
	RenderStatistics result = impl_->statistics;
	result.meshes = impl_->meshes.size();
	result.materials = impl_->materials.size();
	return result;
}

void LightVulkanBackend::requestScreenshot(std::filesystem::path const& path)
{
	Impl* const impl = impl_.get();
	impl_->graphics->requestFrameCapture([path, impl](lg::CapturedFrame const& frame)
		{
			std::string error;
			if (!frame.ok)
			{
				std::cerr << "Screenshot failed: " << frame.error << "\n";
			}
			else if (!lg::VkApp::writePng(path.string(), frame, &error))
			{
				std::cerr << "Screenshot could not be written to " << path << ": " << error << "\n";
			}
			else
			{
				++impl->screenshotsWritten;
			}
		});
}

std::uint32_t LightVulkanBackend::screenshotsWritten() const noexcept
{
	return impl_->screenshotsWritten;
}

}
