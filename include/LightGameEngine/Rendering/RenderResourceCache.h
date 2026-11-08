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
