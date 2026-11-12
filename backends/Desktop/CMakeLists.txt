find_package(CURL REQUIRED)
add_library(LightGameDesktop STATIC src/FeedbackForm.cpp)
add_library(LightGame::Desktop ALIAS LightGameDesktop)
set_target_properties(LightGameDesktop PROPERTIES EXPORT_NAME Desktop)
target_compile_features(LightGameDesktop PUBLIC cxx_std_20)
target_include_directories(LightGameDesktop PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(LightGameDesktop PUBLIC
	LightGame::Licensing LightVulkanGraphics::LightVulkanGraphics LightVulkanGraphics::UI
	CURL::libcurl)
lge_configure_target(LightGameDesktop)
