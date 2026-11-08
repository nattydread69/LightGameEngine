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

#include <glm/glm.hpp>

#include <cstdint>
#include <filesystem>
#include <string>
#include <variant>
#include <vector>

namespace lge
{

// Built-in unit-sized shapes centred on the origin (a cube spans -0.5..0.5; a sphere
// has diameter 1; cylinders, cones and capsules are 1 tall along +Y). Scale them with
// the object transform. Backends may render these with shared instanced geometry.
enum class PrimitiveShape : std::uint8_t
{
	Cube,
	Sphere,
	Cylinder,
	Cone,
	Capsule,
	// Hexagonal prism, 1 tall along +Y, filling the same unit footprint as a cylinder.
	Hex
};

struct MeshVertex
{
	glm::vec3 position {0.0f};
	glm::vec3 normal {0.0f, 1.0f, 0.0f};
	glm::vec4 color {1.0f};
};

// CPU-side indexed triangle list. Counter-clockwise front faces.
struct MeshData
{
	std::vector<MeshVertex> vertices;
	std::vector<std::uint32_t> indices;
};

// Throws std::invalid_argument for empty data, incomplete triangles, out-of-range
// indices or non-finite attributes. Both backends validate with this function.
void validateMeshData(MeshData const& data);

struct MeshDescription
{
	std::variant<PrimitiveShape, MeshData> geometry = PrimitiveShape::Cube;
	// For diagnostics only.
	std::string name;

	[[nodiscard]] static MeshDescription primitive(PrimitiveShape const shape, std::string name = {})
	{
		return MeshDescription {shape, std::move(name)};
	}

	[[nodiscard]] static MeshDescription custom(MeshData data, std::string name = {})
	{
		return MeshDescription {std::move(data), std::move(name)};
	}

	[[nodiscard]] bool isPrimitive() const noexcept
	{
		return std::holds_alternative<PrimitiveShape>(geometry);
	}
};

struct MaterialDescription
{
	// Linear RGBA multiplied with the mesh's vertex colours.
	glm::vec4 baseColor {1.0f};
	// 0 = fully lit, 1 = drawn at full colour regardless of lighting.
	float emissive = 0.0f;
	// Optional texture as a resolved filesystem path (see AssetManager::resolve).
	// Supported on primitive meshes; empty means untextured.
	std::filesystem::path texture;
	glm::vec2 textureTiling {1.0f};
	std::string name;
};

}
