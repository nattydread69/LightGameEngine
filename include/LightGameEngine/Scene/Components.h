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

#include "LightGameEngine/Math/Transform.h"
#include "LightGameEngine/Physics/CollisionWorld2D.h"
#include "LightGameEngine/Rendering/RenderScene.h"
#include "LightGameEngine/Rendering/ResourceHandle.h"

#include <string>

namespace lge
{

// Authoritative world-space transform. There is no transform hierarchy yet: every
// transform is in world space, which keeps destruction and update order trivial.
using TransformComponent = Transform;

// Transform at the start of the current simulation tick. Maintained by the engine
// for rendered entities and cameras so frames can interpolate between ticks; it is
// visual state only and must not drive gameplay.
struct PreviousTransformComponent
{
	Transform value;
};

// Makes an entity follow a parent: after each tick's physics, the entity's world
// transform becomes parent transform * local. This is the engine's only form of
// transform hierarchy, intended for rigid visual parts (a robot's head, a turret).
// Chains are resolved parent-first; cycles are detected and broken; destroying a
// parent destroys its attached children at the same flush.
struct AttachmentComponent
{
	Entity parent;
	Transform local;
};

struct NameComponent
{
	std::string value;
};

struct VelocityComponent
{
	// Metres per second, world space.
	glm::dvec3 linear {0.0};
	// Radians per second about the world-space axis given by the vector's direction.
	glm::dvec3 angular {0.0};
};

// Presentation only: removing it never affects simulation.
struct MeshRendererComponent
{
	MeshHandle mesh;
	MaterialHandle material;
	bool visible = true;
};

struct CameraComponent
{
	ProjectionType projection = ProjectionType::Perspective;
	double verticalFieldOfViewDegrees = 60.0;
	// Orthographic only: world units visible vertically.
	double orthographicHeight = 20.0;
	double nearPlane = 0.1;
	double farPlane = 1000.0;
};

// Light emitted along the entity's forward (-Z) axis for directional and spot lights.
struct LightComponent
{
	LightType type = LightType::Directional;
	glm::vec3 color {1.0f};
	float intensity = 1.0f;
	float range = 10.0f;
	float innerConeRadians = 0.35f;
	float outerConeRadians = 0.5f;
	bool castsShadow = false;
};

// 2D collider on the XZ ground plane (see Physics/Plane.h). Static colliders never
// move; dynamic ones are pushed out of overlaps; triggers only report overlaps.
struct Collider2DComponent
{
	ColliderShape2D shape;
	CollisionLayers layer = 1;
	CollisionLayers mask = allCollisionLayers;
	bool isStatic = false;
	bool isTrigger = false;
};

}
