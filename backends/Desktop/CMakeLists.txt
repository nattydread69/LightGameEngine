find_package(CURL REQUIRED)
# No STATIC keyword: BUILD_SHARED_LIBS decides, as for the engine libraries (lge_add_library), so an application can
# link the desktop bridge as a shared library to meet the LGPL without relinking.
add_library(LightGameDesktop src/FeedbackForm.cpp)
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
