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
