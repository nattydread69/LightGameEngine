#pragma once

#include "LightGameEngine/Rendering/IRenderer.h"
#include "LightGameEngine/Rendering/ResourceHandle.h"

#include <cstdint>
#include <vector>

namespace lge
{

// Renderer that draws nothing. It creates no window and no graphics context, but it
// validates every submission exactly as a real backend would (handle validity, mesh
// data) and records statistics, so engine and game logic can be tested on machines
// without a GPU. It does not emulate any part of a graphics pipeline.
class NullRenderer final : public IRenderer
{
public:
	MeshHandle createMesh(MeshDescription const& description) override;
	void destroyMesh(MeshHandle mesh) override;
	MaterialHandle createMaterial(MaterialDescription const& description) override;
	void destroyMaterial(MaterialHandle material) override;

	void resize(std::uint32_t width, std::uint32_t height) override;
	void render(RenderScene const& scene) override;
	[[nodiscard]] RenderStatistics statistics() const override;

	[[nodiscard]] std::uint32_t width() const noexcept
	{
		return width_;
	}
	[[nodiscard]] std::uint32_t height() const noexcept
	{
		return height_;
	}
	[[nodiscard]] std::uint64_t resizeCount() const noexcept
	{
		return resizeCount_;
	}
	// Copy of the most recent submission, for test assertions.
	[[nodiscard]] RenderScene const& lastScene() const noexcept
	{
		return lastScene_;
	}
	[[nodiscard]] MeshDescription const* mesh(MeshHandle handle) const noexcept
	{
		return meshes_.get(handle);
	}
	[[nodiscard]] MaterialDescription const* material(MaterialHandle handle) const noexcept
	{
		return materials_.get(handle);
	}

private:
	HandlePool<MeshTag, MeshDescription> meshes_;
	HandlePool<MaterialTag, MaterialDescription> materials_;
	RenderStatistics statistics_;
	RenderScene lastScene_;
	std::uint32_t width_ = 0;
	std::uint32_t height_ = 0;
	std::uint64_t resizeCount_ = 0;
};

}
