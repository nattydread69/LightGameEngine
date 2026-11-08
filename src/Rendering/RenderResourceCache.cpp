#include "LightGameEngine/Rendering/RenderResourceCache.h"

#include <array>
#include <string_view>

namespace lge
{

namespace
{

std::string primitiveKey(PrimitiveShape const shape)
{
	static constexpr std::array<std::string_view, 5> names {"cube", "sphere", "cylinder", "cone", "capsule"};
	return std::string {"<primitive>/"} + std::string {names[static_cast<std::size_t>(shape)]};
}

}

RenderResourceCache::RenderResourceCache(IRenderer& renderer)
	: renderer_(&renderer)
{
}

RenderResourceCache::~RenderResourceCache()
{
	releaseAll();
}

MeshHandle RenderResourceCache::mesh(std::string const& key, std::function<MeshDescription()> const& create)
{
	if (auto const found = meshes_.find(key); found != meshes_.end())
	{
		return found->second;
	}
	MeshHandle const handle = renderer_->createMesh(create());
	meshes_.emplace(key, handle);
	return handle;
}

MeshHandle RenderResourceCache::primitive(PrimitiveShape const shape)
{
	std::string const key = primitiveKey(shape);
	return mesh(key, [shape, &key]()
		{
			return MeshDescription::primitive(shape, key);
		});
}

MaterialHandle RenderResourceCache::material(std::string const& key, MaterialDescription const& description)
{
	if (auto const found = materials_.find(key); found != materials_.end())
	{
		return found->second;
	}
	MaterialDescription named = description;
	if (named.name.empty())
	{
		named.name = key;
	}
	MaterialHandle const handle = renderer_->createMaterial(named);
	materials_.emplace(key, handle);
	return handle;
}

bool RenderResourceCache::containsMesh(std::string const& key) const
{
	return meshes_.contains(key);
}

bool RenderResourceCache::containsMaterial(std::string const& key) const
{
	return materials_.contains(key);
}

void RenderResourceCache::releaseMesh(std::string const& key)
{
	if (auto const found = meshes_.find(key); found != meshes_.end())
	{
		renderer_->destroyMesh(found->second);
		meshes_.erase(found);
	}
}

void RenderResourceCache::releaseMaterial(std::string const& key)
{
	if (auto const found = materials_.find(key); found != materials_.end())
	{
		renderer_->destroyMaterial(found->second);
		materials_.erase(found);
	}
}

void RenderResourceCache::releaseAll()
{
	for (auto const& [key, handle] : meshes_)
	{
		renderer_->destroyMesh(handle);
	}
	for (auto const& [key, handle] : materials_)
	{
		renderer_->destroyMaterial(handle);
	}
	meshes_.clear();
	materials_.clear();
}

}
