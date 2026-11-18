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

#include "LightGameEngine/Rendering/RenderResources.h"
#include "LightGameEngine/Rendering/RenderScene.h"

#include <glm/gtc/constants.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/quaternion.hpp>

#include <cmath>
#include <stdexcept>
#include <string>

namespace lge
{

glm::dvec3 RenderCamera::forward() const
{
	return orientation * glm::dvec3 {0.0, 0.0, -1.0};
}

glm::dvec3 RenderCamera::up() const
{
	return orientation * glm::dvec3 {0.0, 1.0, 0.0};
}

glm::dmat4 RenderCamera::viewMatrix() const
{
	return glm::lookAt(position, position + forward(), up());
}

glm::dmat4 RenderCamera::projectionMatrix(double const aspectRatio) const
{
	if (projection == ProjectionType::Orthographic)
	{
		double const halfHeight = orthographicHeight * 0.5;
		double const halfWidth = halfHeight * aspectRatio;
		return glm::ortho(-halfWidth, halfWidth, -halfHeight, halfHeight, nearPlane, farPlane);
	}
	return glm::perspective(glm::radians(verticalFieldOfViewDegrees), aspectRatio, nearPlane, farPlane);
}

SpriteTransform computeSpriteTransform(RenderSprite const& sprite, RenderCamera const& camera)
{
	glm::dquat base {1.0, 0.0, 0.0, 0.0};
	glm::dquat spin = glm::angleAxis(static_cast<double>(sprite.rotationRadians), glm::dvec3 {0.0, 0.0, 1.0});
	switch (sprite.orientation)
	{
	case SpriteOrientation::Billboard:
		base = camera.orientation;
		break;
	case SpriteOrientation::Ground:
		// Tip the quad flat (+Z becomes +Y, +Y becomes -Z), then turn it about the vertical.
		base = glm::angleAxis(static_cast<double>(sprite.rotationRadians), glm::dvec3 {0.0, 1.0, 0.0})
			* glm::angleAxis(-glm::half_pi<double>(), glm::dvec3 {1.0, 0.0, 0.0});
		spin = glm::dquat {1.0, 0.0, 0.0, 0.0};
		break;
	case SpriteOrientation::Oriented:
		base = sprite.rotation;
		break;
	}
	SpriteTransform result;
	result.rotation = glm::normalize(base * spin);
	result.size = sprite.size;
	// The quad is built centred, so move it by the offset from the pivot to the middle.
	glm::dvec3 const offset {(0.5 - sprite.pivot.x) * sprite.size.x, (0.5 - sprite.pivot.y) * sprite.size.y, 0.0};
	result.center = sprite.position + result.rotation * offset;
	return result;
}

glm::vec4 atlasFrame(std::uint32_t const columns, std::uint32_t const rows, std::uint32_t const index)
{
	if (columns == 0 || rows == 0)
	{
		throw std::invalid_argument("atlasFrame: columns and rows must be positive");
	}
	std::uint32_t const frame = index % (columns * rows);
	float const width = 1.0f / static_cast<float>(columns);
	float const height = 1.0f / static_cast<float>(rows);
	float const u = static_cast<float>(frame % columns) * width;
	float const v = static_cast<float>(frame / columns) * height;
	return {u, v, u + width, v + height};
}

void validateTextureDescription(TextureDescription const& description)
{
	bool const hasPath = !description.path.empty();
	bool const hasPixels = !description.rgba8.empty();
	if (hasPath == hasPixels)
	{
		throw std::invalid_argument("TextureDescription: give exactly one of path and rgba8");
	}
	if (hasPixels)
	{
		if (description.width == 0 || description.height == 0 || description.width > 16384 || description.height > 16384)
		{
			throw std::invalid_argument("TextureDescription: width and height must be between 1 and 16384");
		}
		std::size_t const expected = static_cast<std::size_t>(description.width) * description.height * 4;
		if (description.rgba8.size() != expected)
		{
			throw std::invalid_argument("TextureDescription: rgba8 holds " + std::to_string(description.rgba8.size()) + " bytes, expected " + std::to_string(expected));
		}
	}
}

void validateMeshData(MeshData const& data)
{
	if (data.vertices.empty() || data.indices.empty())
	{
		throw std::invalid_argument("MeshData: vertices and indices must not be empty");
	}
	if (data.indices.size() % 3 != 0)
	{
		throw std::invalid_argument("MeshData: index count must be a multiple of 3");
	}
	for (std::uint32_t const index : data.indices)
	{
		if (index >= data.vertices.size())
		{
			throw std::invalid_argument("MeshData: index " + std::to_string(index) + " is out of range");
		}
	}
	auto const finite = [](auto const& v)
	{
		for (int i = 0; i < v.length(); ++i)
		{
			if (!std::isfinite(v[i]))
			{
				return false;
			}
		}
		return true;
	};
	for (MeshVertex const& vertex : data.vertices)
	{
		if (!finite(vertex.position) || !finite(vertex.normal) || !finite(vertex.color))
		{
			throw std::invalid_argument("MeshData: vertex attributes must be finite");
		}
	}
}

}
