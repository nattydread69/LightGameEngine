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

#include "LightGameEngine/Rendering/RenderResources.h"
#include "LightGameEngine/Rendering/RenderScene.h"

#include <cstddef>
#include <cstdint>

namespace lge
{

struct RenderStatistics
{
	std::uint64_t framesRendered = 0;
	// Counts for the most recent frame.
	std::size_t submittedObjects = 0;
	std::size_t drawnObjects = 0;
	// Objects skipped because their mesh or material handle was stale or invalid.
	std::size_t rejectedObjects = 0;
	std::size_t lights = 0;
	std::size_t texts = 0;
	// Live resources.
	std::size_t meshes = 0;
	std::size_t materials = 0;
};

// Backend-independent renderer. The game engine talks only to this interface.
//
// Ownership: the renderer owns every mesh and material it creates; callers hold
// handles. Destroying a resource invalidates its handle immediately, and objects
// still referencing it are skipped (and counted as rejected) rather than causing
// errors. Backends must defer releasing GPU memory until frames that may still
// reference it have finished executing.
class IRenderer
{
public:
	IRenderer() = default;
	IRenderer(IRenderer const&) = delete;
	IRenderer& operator=(IRenderer const&) = delete;
	virtual ~IRenderer() = default;

	virtual MeshHandle createMesh(MeshDescription const& description) = 0;
	virtual void destroyMesh(MeshHandle mesh) = 0;
	virtual MaterialHandle createMaterial(MaterialDescription const& description) = 0;
	virtual void destroyMaterial(MaterialHandle material) = 0;

	// Informs the renderer of the drawable size in pixels.
	virtual void resize(std::uint32_t width, std::uint32_t height) = 0;
	// Submits one frame. The scene is only read during the call.
	virtual void render(RenderScene const& scene) = 0;

	[[nodiscard]] virtual RenderStatistics statistics() const = 0;
};

}
