#pragma once

#include "LightGameEngine/Platform/IPlatform.h"
#include "LightGameEngine/Rendering/IRenderer.h"

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>

namespace lge
{

struct LightVulkanBackendConfig
{
	std::string title = "LightGameEngine";
	std::uint32_t width = 1280;
	std::uint32_t height = 720;
	// Directory of LightVulkanGraphics' compiled shaders. Empty uses the library's own
	// discovery (LIGHT_VULKAN_GRAPHICS_SHADER_PATH, build tree, install prefix).
	std::filesystem::path shaderDirectory;
	// Forward LightVulkanGraphics' console diagnostics (device selection etc.).
	bool graphicsConsoleOutput = false;
};

// Adapter between the engine and LightVulkanGraphics. One object fulfils both roles
// that the graphics library combines, so there is a single owner:
//   * IPlatform - the window, the frame loop and keyboard/mouse input. The window is
//     created and owned by LightVulkanGraphics; this class owns the library instance.
//   * IRenderer - translates RenderScene snapshots into LightVulkanGraphics objects,
//     custom meshes, lights and screen text.
// No Vulkan, GLFW or LightVulkanGraphics type appears in this header.
//
// Must be created, used and destroyed on one thread (a LightVulkanGraphics rule).
class LightVulkanBackend final : public IPlatform, public IRenderer
{
public:
	explicit LightVulkanBackend(LightVulkanBackendConfig const& config = {});
	~LightVulkanBackend() override;

	// IPlatform
	void run(FrameFunction const& frame) override;
	[[nodiscard]] double timeSeconds() const override;
	void pollInput(InputState& input) override;
	[[nodiscard]] WindowExtent framebufferExtent() const override;
	void requestClose() override;

	// IRenderer
	MeshHandle createMesh(MeshDescription const& description) override;
	void destroyMesh(MeshHandle mesh) override;
	MaterialHandle createMaterial(MaterialDescription const& description) override;
	void destroyMaterial(MaterialHandle material) override;
	void resize(std::uint32_t width, std::uint32_t height) override;
	void render(RenderScene const& scene) override;
	[[nodiscard]] RenderStatistics statistics() const override;

	// Writes the next presented frame to a PNG file (for documentation and automated
	// visual checks). The file is written during a later frame.
	void requestScreenshot(std::filesystem::path const& path);
	// Number of screenshots written successfully so far.
	[[nodiscard]] std::uint32_t screenshotsWritten() const noexcept;

private:
	struct Impl;
	std::unique_ptr<Impl> impl_;
};

}
