# The only engine target that depends on Vulkan, GLFW and LightVulkanGraphics. Those
# dependencies are PRIVATE: consumers of LightGame::RendererVulkan see engine types only.
add_library(LightGameRendererVulkan STATIC
	src/LightVulkanBackend.cpp
	src/GlfwKeyMap.h)
add_library(LightGame::RendererVulkan ALIAS LightGameRendererVulkan)
target_include_directories(LightGameRendererVulkan
	PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/src)
target_link_libraries(LightGameRendererVulkan
	PUBLIC LightGame::Rendering LightGame::Core
	PRIVATE LightVulkanGraphics::LightVulkanGraphics glfw)
lge_configure_target(LightGameRendererVulkan)
# LightVulkanGraphics headers are third-party code; keep their warnings out of ours.
get_target_property(_lvg_includes LightVulkanGraphics::LightVulkanGraphics INTERFACE_INCLUDE_DIRECTORIES)
if(_lvg_includes)
	target_include_directories(LightGameRendererVulkan SYSTEM PRIVATE ${_lvg_includes})
endif()
