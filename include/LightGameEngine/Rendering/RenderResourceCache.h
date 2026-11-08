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

#include "LightGameEngine/Rendering/IRenderer.h"

#include <functional>
#include <string>
#include <unordered_map>

namespace lge
{

// Creates renderer resources on first request and returns the cached handle for every
// later request with the same key, so a mesh or material shared by many entities is
// created once. The cache owns the handles it creates: release() or destruction
// destroys them through the renderer, which must outlive the cache.
//
// Invalidation is explicit: call forget(key) after destroying a resource elsewhere, or
// release(key) to destroy it and drop the entry. Keys are typically asset paths.
class RenderResourceCache
{
public:
	explicit RenderResourceCache(IRenderer& renderer);
	RenderResourceCache(RenderResourceCache const&) = delete;
	RenderResourceCache& operator=(RenderResourceCache const&) = delete;
	~RenderResourceCache();

	MeshHandle mesh(std::string const& key, std::function<MeshDescription()> const& create);
	// Convenience: the cached handle for a built-in primitive shape.
	MeshHandle primitive(PrimitiveShape shape);
	MaterialHandle material(std::string const& key, MaterialDescription const& description);

	[[nodiscard]] bool containsMesh(std::string const& key) const;
	[[nodiscard]] bool containsMaterial(std::string const& key) const;
	[[nodiscard]] std::size_t meshCount() const noexcept
	{
		return meshes_.size();
	}
	[[nodiscard]] std::size_t materialCount() const noexcept
	{
		return materials_.size();
	}

	void releaseMesh(std::string const& key);
	void releaseMaterial(std::string const& key);
	void releaseAll();

private:
	IRenderer* renderer_;
	std::unordered_map<std::string, MeshHandle> meshes_;
	std::unordered_map<std::string, MaterialHandle> materials_;
};

}
