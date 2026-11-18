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
#include <array>
#include <fstream>
#include <functional>
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
	case PrimitiveShape::Hex:
		return lg::ShapeType::HEX;
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
	// Layer and sort key: sprites go in the transparent layer, ordered back to front.
	lg::DrawOptions options {};

	bool operator==(CustomDraw const& other) const
	{
		return mesh.index == other.mesh.index && mesh.generation == other.mesh.generation &&
			material.index == other.material.index && material.generation == other.material.generation &&
			position == other.position && rotation == other.rotation && scale == other.scale &&
			options.layer == other.options.layer && options.sortKey == other.options.sortKey;
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

// Work that must wait until the GPU can no longer be using a resource (destroying a sprite
// texture and the material built on it, in that order of dependency).
struct PendingAction
{
	std::function<void()> action;
	std::uint64_t releaseFrame = 0;
};

// A sprite texture on the GPU, with the alpha-blended material that draws it (made on first use).
struct TextureRecord
{
	TextureDescription description;
	lg::Texture2DHandle gpu;
	// Indexed by "lit": the unlit material draws the artwork as is, the lit one takes LightVulkanGraphics' shading.
	std::array<lg::MaterialHandle, 2> material {};
	std::array<bool, 2> hasMaterial {};
};

// Sprite quads differ only by UV rectangle and tint, so one baked mesh serves every sprite that
// shares them. Meshes unused for a while are released.
struct SpriteMeshKey
{
	std::array<float, 4> uv {};
	std::uint32_t tint = 0;

	friend bool operator<(SpriteMeshKey const& a, SpriteMeshKey const& b)
	{
		return std::tie(a.uv, a.tint) < std::tie(b.uv, b.tint);
	}
};

struct SpriteMeshEntry
{
	lg::MeshHandle mesh;
	std::uint64_t lastUsedFrame = 0;
};

constexpr std::uint64_t spriteMeshIdleFrames = 240;

std::uint32_t packTint(glm::vec4 const& tint)
{
	std::uint32_t packed = 0;
	for (int i = 0; i < 4; ++i)
	{
		float const clamped = std::clamp(tint[i], 0.0f, 1.0f);
		packed |= static_cast<std::uint32_t>(std::lround(clamped * 255.0f)) << (8 * i);
	}
	return packed;
}

// GLFW reports the wheel only through a callback with no user data (the window user
// pointer belongs to LightVulkanGraphics), so the one live backend is reached through
// this pointer. The callback forwards every event to the callback it replaced.
struct ScrollRelay
{
	double* accumulator = nullptr;
	GLFWscrollfun previous = nullptr;
};

ScrollRelay scrollRelay;

void scrollThunk(GLFWwindow* const window, double const xOffset, double const yOffset)
{
	if (scrollRelay.accumulator != nullptr)
	{
		*scrollRelay.accumulator += yOffset;
	}
	if (scrollRelay.previous != nullptr)
	{
		scrollRelay.previous(window, xOffset, yOffset);
	}
}

}

struct LightVulkanBackend::Impl
{
	std::unique_ptr<lg::lightVulkanGraphics> graphics;
	GLFWwindow* window = nullptr;

	HandlePool<MeshTag, MeshDescription> meshes;
	HandlePool<MaterialTag, MaterialRecord> materials;
	HandlePool<TextureTag, TextureRecord> textures;
	std::map<SpriteMeshKey, SpriteMeshEntry> spriteMeshes;
	std::vector<PendingAction> pendingActions;

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
	double pendingScroll = 0.0;
	bool scrollHooked = false;

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

	void releaseRetiredActions()
	{
		auto const ready = [this](PendingAction const& pending)
		{
			return pending.releaseFrame <= frameNumber;
		};
		for (PendingAction const& pending : pendingActions)
		{
			if (ready(pending))
			{
				pending.action();
			}
		}
		pendingActions.erase(std::remove_if(pendingActions.begin(), pendingActions.end(), ready), pendingActions.end());
	}

	lg::MeshHandle spriteMesh(RenderSprite const& sprite)
	{
		// Flipping is just swapping the UV corners.
		float u0 = sprite.uvRect.x;
		float v0 = sprite.uvRect.y;
		float u1 = sprite.uvRect.z;
		float v1 = sprite.uvRect.w;
		if (sprite.flipX)
		{
			std::swap(u0, u1);
		}
		if (sprite.flipY)
		{
			std::swap(v0, v1);
		}
		SpriteMeshKey const key {{u0, v0, u1, v1}, packTint(sprite.tint)};
		auto const found = spriteMeshes.find(key);
		if (found != spriteMeshes.end())
		{
			found->second.lastUsedFrame = frameNumber;
			return found->second.mesh;
		}
		// The tint is baked into the vertex colours, which the textured shader multiplies in.
		glm::vec4 const colour {sprite.tint};
		lg::MeshData quad;
		auto const corner = [&](float const x, float const y, float const u, float const v)
		{
			lg::MeshVertex vertex;
			vertex.position = {x, y, 0.0f};
			vertex.normal = {0.0f, 0.0f, 1.0f};
			vertex.color = colour;
			vertex.uv = {u, v};
			quad.vertices.push_back(vertex);
		};
		corner(-0.5f, -0.5f, u0, v1);
		corner(0.5f, -0.5f, u1, v1);
		corner(0.5f, 0.5f, u1, v0);
		corner(-0.5f, 0.5f, u0, v0);
		quad.indices = {0, 1, 2, 0, 2, 3};
		lg::MeshHandle const handle = graphics->createStaticMesh(quad);
		spriteMeshes.emplace(key, SpriteMeshEntry {handle, frameNumber});
		return handle;
	}

	lg::MaterialHandle spriteMaterial(TextureRecord& record, bool const lit)
	{
		std::size_t const which = lit ? 1 : 0;
		if (!record.hasMaterial[which])
		{
			lg::MaterialDescription description;
			// Unlit needs LightVulkanGraphics 3.5; older libraries fall back to their fixed key-light shading.
#ifdef LVG_HAS_TEXTURED_UNLIT
			description.shaderProgram.selection = lit ? lg::ShaderSelection::Textured : lg::ShaderSelection::TexturedUnlit;
#else
			description.shaderProgram.selection = lg::ShaderSelection::Textured;
#endif
			description.textures[0] = record.gpu;
			// Soft edges need blending; sprites test depth against the scene but do not write it, so
			// overlapping translucent sprites do not cut holes in each other. Both faces are visible.
			description.alphaBlendingEnabled = true;
			description.depthWriteEnabled = false;
			description.cullMode = lg::CullMode::None;
			record.material[which] = graphics->createMaterial(description);
			record.hasMaterial[which] = true;
		}
		return record.material[which];
	}

	// Queues this frame's sprites as transparent mesh draws, ordered by sortOrder and then far to near.
	void collectSprites(RenderScene const& scene, std::size_t& drawn)
	{
		for (RenderSprite const& sprite : scene.sprites)
		{
			TextureRecord* const record = textures.get(sprite.texture);
			if (record == nullptr)
			{
				continue;
			}
			SpriteTransform const placed = computeSpriteTransform(sprite, scene.camera);
			float const distance = static_cast<float>(glm::length(placed.center - scene.camera.position));
			CustomDraw draw {spriteMesh(sprite), spriteMaterial(*record, sprite.lit), relative(placed.center), glm::quat {placed.rotation},
				glm::vec3 {placed.size.x, placed.size.y, 1.0f}};
			draw.options.layer = lg::RenderLayer::Transparent;
			draw.options.sortKey = static_cast<float>(sprite.sortOrder) * 1.0e5f - distance;
			customDraws.push_back(draw);
			++drawn;
		}
		// Release quads nobody has asked for in a while.
		for (auto it = spriteMeshes.begin(); it != spriteMeshes.end();)
		{
			if (frameNumber - it->second.lastUsedFrame > spriteMeshIdleFrames)
			{
				pendingReleases.push_back(PendingMeshRelease {it->second.mesh, frameNumber + framesInFlight + 1});
				it = spriteMeshes.erase(it);
				customDrawsDirty = true;
			}
			else
			{
				++it;
			}
		}
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
			graphics->drawMesh(draw.mesh, transform, draw.material, draw.options);
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
			if (light.castsShadow)
			{
				if (light.shadowStrength > 0.0f) source.shadowStrength = light.shadowStrength;
				if (light.shadowOrthographicSize > 0.0f) source.shadowOrthoSize = light.shadowOrthographicSize;
				if (light.shadowFar > 0.0f) source.shadowFar = light.shadowFar;
				if (light.shadowMapSize > 0) source.shadowMapSize = light.shadowMapSize;
			}
			std::size_t const lightIndex = graphics->addLight(source);
			if (light.castsShadow && light.shadowCenterPinned)
			{
				graphics->setLightShadowCenter(lightIndex, relative(light.shadowCenter));
			}
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
					slot.last.scale != text.scale || slot.last.color != text.color ||
						slot.last.background != text.background || slot.last.backgroundColor != text.backgroundColor)
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
		description.backgroundEnabled = text.background;
		description.backgroundColor = text.backgroundColor;
		// Draw from the GUI's TrueType atlas when the library has one (the default).
		description.useFontAtlas = true;
		description.monospace = text.monospace;
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
	if (scrollRelay.accumulator == nullptr)
	{
		scrollRelay.accumulator = &impl_->pendingScroll;
		scrollRelay.previous = glfwSetScrollCallback(impl_->window, &scrollThunk);
		impl_->scrollHooked = true;
	}
}

LightVulkanBackend::~LightVulkanBackend()
{
	if (impl_->scrollHooked)
	{
		glfwSetScrollCallback(impl_->window, scrollRelay.previous);
		scrollRelay = {};
	}
}

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

namespace
{

// GLFW numbers gamepad buttons and axes identically to GamepadButton / GamepadAxis, so
// the translation is an index cast. Only joysticks with a gamepad mapping count; a
// device GLFW cannot map to the standard layout is reported as disconnected.
void pollGamepads(InputState& input)
{
	static_assert(GLFW_GAMEPAD_BUTTON_LAST + 1 == static_cast<int>(gamepadButtonCount));
	static_assert(GLFW_GAMEPAD_AXIS_LAST + 1 == static_cast<int>(gamepadAxisCount));
	for (std::size_t pad = 0; pad < maxGamepads; ++pad)
	{
		int const jid = GLFW_JOYSTICK_1 + static_cast<int>(pad);
		GLFWgamepadstate state {};
		if (glfwJoystickIsGamepad(jid) != GLFW_TRUE || glfwGetGamepadState(jid, &state) != GLFW_TRUE)
		{
			input.setGamepadConnected(pad, false);
			continue;
		}
		input.setGamepadConnected(pad, true);
		for (std::size_t button = 0; button < gamepadButtonCount; ++button)
		{
			input.setGamepadButtonDown(pad, static_cast<GamepadButton>(button), state.buttons[button] == GLFW_PRESS);
		}
		for (std::size_t axis = 0; axis < gamepadAxisCount; ++axis)
		{
			double value = state.axes[axis];
			if (axis == static_cast<std::size_t>(GamepadAxis::LeftY) || axis == static_cast<std::size_t>(GamepadAxis::RightY))
			{
				value = -value; // GLFW reports stick-up as negative; the engine uses +Y up.
			}
			else if (axis == static_cast<std::size_t>(GamepadAxis::LeftTrigger) || axis == static_cast<std::size_t>(GamepadAxis::RightTrigger))
			{
				value = (value + 1.0) * 0.5; // GLFW triggers span -1 (released) to +1.
			}
			input.setGamepadAxis(pad, static_cast<GamepadAxis>(axis), value);
		}
	}
}

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
	// GLFW reports the cursor in window (screen) coordinates, but InputState promises
	// framebuffer pixels, the space screen text and picking use. The two differ when the
	// platform scales windows (HiDPI), so rescale by the framebuffer-to-window ratio.
	int windowWidth = 0;
	int windowHeight = 0;
	glfwGetWindowSize(window, &windowWidth, &windowHeight);
	WindowExtent const extent = framebufferExtent();
	if (windowWidth > 0 && windowHeight > 0 && extent.width > 0 && extent.height > 0)
	{
		x *= static_cast<double>(extent.width) / windowWidth;
		y *= static_cast<double>(extent.height) / windowHeight;
	}
	input.setMousePosition(glm::dvec2 {x, y});
	pollGamepads(input);
	input.addMouseScroll(impl_->pendingScroll);
	impl_->pendingScroll = 0.0;
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

TextureHandle LightVulkanBackend::createTexture(TextureDescription const& description)
{
	validateTextureDescription(description);
	Impl& impl = *impl_;
	lg::Texture2DDescription gpuDescription;
	gpuDescription.format = description.srgb ? lg::Texture2DFormat::RGBA8_SRGB : lg::Texture2DFormat::RGBA8_UNORM;
	gpuDescription.repeat = false;
#ifdef LVG_HAS_TEXTURE_FILTER
	gpuDescription.filter = description.filter == TextureFilter::Nearest ? lg::Texture2DFilter::Nearest : lg::Texture2DFilter::Linear;
#endif
	gpuDescription.generateMipmaps = description.mipmaps;
	TextureRecord record;
	record.description = description;
	if (!description.path.empty())
	{
		std::ifstream file(description.path, std::ios::binary);
		if (!file)
		{
			throw std::runtime_error("LightVulkanBackend::createTexture: cannot open " + description.path.string());
		}
		std::vector<char> const bytes((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
		// Clamped, not repeated: a sprite sheet must not bleed in from the opposite edge.
#ifdef LVG_HAS_TEXTURE_FILTER
		record.gpu = impl.graphics->createTexture2DFromImage(bytes.data(), bytes.size(), gpuDescription);
#else
		// 3.4 cannot choose a filter or skip mipmaps for decoded images.
		record.gpu = impl.graphics->createTexture2DFromImage(bytes.data(), bytes.size(), gpuDescription.format, false);
#endif
		if (!record.gpu.isValid())
		{
			throw std::runtime_error("LightVulkanBackend::createTexture: cannot decode " + description.path.string());
		}
	}
	else
	{
		gpuDescription.width = description.width;
		gpuDescription.height = description.height;
		record.gpu = impl.graphics->createTexture2D(gpuDescription, description.rgba8.data(), description.rgba8.size());
	}
	return impl.textures.insert(std::move(record));
}

void LightVulkanBackend::destroyTexture(TextureHandle const texture)
{
	Impl& impl = *impl_;
	TextureRecord const* const found = impl.textures.get(texture);
	if (found == nullptr)
	{
		throw std::invalid_argument("LightVulkanBackend::destroyTexture: handle is invalid or already destroyed");
	}
	// The material must go before the texture it samples, and neither while a frame in flight may
	// still draw with them.
	TextureRecord const record = *found;
	impl.textures.erase(texture);
	impl.customDrawsDirty = true;
	lg::lightVulkanGraphics* const graphics = impl.graphics.get();
	impl.pendingActions.push_back(PendingAction {[graphics, record]
		{
			for (std::size_t which = 0; which < record.material.size(); ++which)
			{
				if (record.hasMaterial[which])
				{
					graphics->destroyMaterial(record.material[which]);
				}
			}
			graphics->destroyTexture2D(record.gpu);
		}, impl.frameNumber + framesInFlight + 1});
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
	impl.releaseRetiredActions();

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
	std::size_t drawnSprites = 0;
	impl.collectSprites(scene, drawnSprites);
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
	impl.statistics.submittedSprites = scene.sprites.size();
	impl.statistics.drawnSprites = drawnSprites;
	impl.statistics.rejectedSprites = scene.sprites.size() - drawnSprites;
}

RenderStatistics LightVulkanBackend::statistics() const
{
	RenderStatistics result = impl_->statistics;
	result.meshes = impl_->meshes.size();
	result.materials = impl_->materials.size();
	result.textures = impl_->textures.size();
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
