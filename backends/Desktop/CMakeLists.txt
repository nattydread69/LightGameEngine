add_library(LightGameDesktop INTERFACE)
add_library(LightGame::Desktop ALIAS LightGameDesktop)
target_compile_features(LightGameDesktop INTERFACE cxx_std_20)
target_include_directories(LightGameDesktop INTERFACE
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(LightGameDesktop INTERFACE
	LightGame::Licensing LightVulkanGraphics::LightVulkanGraphics LightVulkanGraphics::UI)
