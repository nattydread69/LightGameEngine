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

#include "LightGameEngine/Backends/NullRenderer.h"

#include <stdexcept>

namespace lge
{

MeshHandle NullRenderer::createMesh(MeshDescription const& description)
{
	if (MeshData const* const data = std::get_if<MeshData>(&description.geometry))
	{
		validateMeshData(*data);
	}
	return meshes_.insert(description);
}

void NullRenderer::destroyMesh(MeshHandle const mesh)
{
	if (!meshes_.erase(mesh))
	{
		throw std::invalid_argument("NullRenderer::destroyMesh: handle is invalid or already destroyed");
	}
}

MaterialHandle NullRenderer::createMaterial(MaterialDescription const& description)
{
	return materials_.insert(description);
}

void NullRenderer::destroyMaterial(MaterialHandle const material)
{
	if (!materials_.erase(material))
	{
		throw std::invalid_argument("NullRenderer::destroyMaterial: handle is invalid or already destroyed");
	}
}

void NullRenderer::resize(std::uint32_t const width, std::uint32_t const height)
{
	width_ = width;
	height_ = height;
	++resizeCount_;
}

void NullRenderer::render(RenderScene const& scene)
{
	std::size_t drawn = 0;
	for (RenderObject const& object : scene.objects)
	{
		if (meshes_.contains(object.mesh) && materials_.contains(object.material))
		{
			++drawn;
		}
	}
	++statistics_.framesRendered;
	statistics_.submittedObjects = scene.objects.size();
	statistics_.drawnObjects = drawn;
	statistics_.rejectedObjects = scene.objects.size() - drawn;
	statistics_.lights = scene.lights.size();
	statistics_.texts = scene.texts.size();
	lastScene_ = scene;
}

RenderStatistics NullRenderer::statistics() const
{
	RenderStatistics result = statistics_;
	result.meshes = meshes_.size();
	result.materials = materials_.size();
	return result;
}

}
