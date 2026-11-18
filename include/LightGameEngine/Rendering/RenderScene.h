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

#include "LightGameEngine/Rendering/ResourceHandle.h"

#include <glm/glm.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cstdint>
#include <string>
#include <vector>

namespace lge
{

// Stable identity of a submitted item across frames (normally a packed Entity id).
// Backends use it to keep per-object GPU state alive between frames instead of
// recreating it, so it must stay the same for as long as the item exists.
using RenderObjectId = std::uint64_t;

enum class ProjectionType : std::uint8_t
{
	Perspective,
	Orthographic
};

// Camera state for one frame. The camera looks along its local -Z axis with +Y up.
struct RenderCamera
{
	ProjectionType projection = ProjectionType::Perspective;
	glm::dvec3 position {0.0, 0.0, 5.0};
	glm::dquat orientation {1.0, 0.0, 0.0, 0.0};
	double verticalFieldOfViewDegrees = 60.0;
	// Orthographic only: world-space height of the visible area.
	double orthographicHeight = 10.0;
	double nearPlane = 0.1;
	double farPlane = 1000.0;

	[[nodiscard]] glm::dvec3 forward() const;
	[[nodiscard]] glm::dvec3 up() const;
	[[nodiscard]] glm::dmat4 viewMatrix() const;
	// OpenGL-style clip space (y up, depth -1..1). Backends adapt it to their API.
	[[nodiscard]] glm::dmat4 projectionMatrix(double aspectRatio) const;
};

struct RenderObject
{
	RenderObjectId id = 0;
	MeshHandle mesh;
	MaterialHandle material;
	// World-space transform, already interpolated for this frame.
	glm::dvec3 position {0.0};
	glm::dquat rotation {1.0, 0.0, 0.0, 0.0};
	glm::dvec3 scale {1.0};
};

// How a sprite's quad is oriented. The quad is drawn in its local XY plane facing +Z, with +Y up
// the image and the origin at its pivot.
enum class SpriteOrientation : std::uint8_t
{
	// Always faces the camera; rotationRadians spins it about the view axis.
	Billboard,
	// Lies flat on the ground (the XZ plane, facing +Y) with the top of the image towards -Z;
	// rotationRadians turns it about +Y. The natural choice for top-down games.
	Ground,
	// Uses `rotation` as given (rotationRadians then spins it about its own +Z).
	Oriented
};

// A textured quad. Drawn alpha-blended after the opaque scene, sorted by sortOrder and then
// back to front, so soft edges and transparent texels composite correctly.
struct RenderSprite
{
	RenderObjectId id = 0;
	TextureHandle texture;
	// World-space position of the pivot.
	glm::dvec3 position {0.0};
	// World size of the whole quad (width, height).
	glm::vec2 size {1.0f};
	// Which point of the quad sits at `position`: (0,0) bottom-left, (0.5,0.5) centre, (0.5,0) bottom-centre.
	glm::vec2 pivot {0.5f};
	// The part of the texture to show, as (u0, v0, u1, v1) with (0,0) the top-left corner of the
	// image. A sprite sheet frame is a sub-rectangle; see atlasFrame().
	glm::vec4 uvRect {0.0f, 0.0f, 1.0f, 1.0f};
	// Multiplies the texture (RGBA), so alpha fades the sprite.
	glm::vec4 tint {1.0f};
	SpriteOrientation orientation = SpriteOrientation::Billboard;
	float rotationRadians = 0.0f;
	glm::dquat rotation {1.0, 0.0, 0.0, 0.0};
	bool flipX = false;
	bool flipY = false;
	// Higher draws on top of lower, whatever the distance.
	int sortOrder = 0;
	// Lit sprites take the backend's scene shading; the default draws the artwork in its own colours.
	bool lit = false;
};

// Where a sprite's centre goes and how its quad is turned, for a given camera.
struct SpriteTransform
{
	glm::dvec3 center {0.0};
	glm::dquat rotation {1.0, 0.0, 0.0, 0.0};
	glm::vec2 size {1.0f};
};

[[nodiscard]] SpriteTransform computeSpriteTransform(RenderSprite const& sprite, RenderCamera const& camera);

// UV rectangle of frame `index` in a sheet of `columns` x `rows` equal cells, counted left to
// right then top to bottom. `index` wraps, so an ever-growing animation counter is fine.
[[nodiscard]] glm::vec4 atlasFrame(std::uint32_t columns, std::uint32_t rows, std::uint32_t index);

enum class LightType : std::uint8_t
{
	Directional,
	Point,
	Spot
};

struct RenderLight
{
	LightType type = LightType::Directional;
	glm::dvec3 position {0.0};
	glm::vec3 direction {0.0f, -1.0f, 0.0f};
	glm::vec3 color {1.0f};
	float intensity = 1.0f;
	float range = 10.0f;
	float innerConeRadians = 0.35f;
	float outerConeRadians = 0.5f;
	bool castsShadow = false;
	// Shadow tuning, used only when castsShadow is set. Zero keeps the backend default.
	float shadowStrength = 0.0f;
	float shadowOrthographicSize = 0.0f;
	float shadowFar = 0.0f;
	std::uint32_t shadowMapSize = 0;
	// Directional lights: fixes the shadow volume's centre instead of following the
	// visible objects. Used when shadowCenterPinned is set.
	glm::dvec3 shadowCenter {0.0};
	bool shadowCenterPinned = false;

	friend bool operator==(RenderLight const&, RenderLight const&) = default;
};

// Screen-space text overlay, positioned in framebuffer pixels from the top-left.
struct ScreenText
{
	RenderObjectId id = 0;
	std::string text;
	glm::vec2 positionPixels {16.0f, 16.0f};
	float scale = 2.0f;
	glm::vec4 color {1.0f};
	// Translucent panel behind the text.
	bool background = false;
	glm::vec4 backgroundColor {0.0f, 0.0f, 0.0f, 0.6f};
	// Use the renderer's monospace face (fixed-width columns). Fixed for the life of the
	// id: the backend reads it when the text is first created.
	bool monospace = false;
};

struct RenderEnvironment
{
	glm::vec3 clearColor {0.02f, 0.03f, 0.05f};
	glm::vec3 ambientLight {0.12f, 0.12f, 0.14f};
};

// Snapshot of everything to draw in one frame. It references persistent resources by
// handle and never owns GPU data, so building it is cheap; the engine reuses one
// instance across frames to avoid reallocating its vectors.
struct RenderScene
{
	RenderCamera camera;
	RenderEnvironment environment;
	std::vector<RenderObject> objects;
	std::vector<RenderLight> lights;
	std::vector<RenderSprite> sprites;
	std::vector<ScreenText> texts;

	// Empties the lists while keeping their capacity.
	void clear() noexcept
	{
		objects.clear();
		lights.clear();
		sprites.clear();
		texts.clear();
	}
};

}
