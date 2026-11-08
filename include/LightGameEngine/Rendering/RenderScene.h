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
	std::vector<ScreenText> texts;

	// Empties the lists while keeping their capacity.
	void clear() noexcept
	{
		objects.clear();
		lights.clear();
		texts.clear();
	}
};

}
