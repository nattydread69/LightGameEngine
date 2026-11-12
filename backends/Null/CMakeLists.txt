add_library(LightGameRendererNull src/NullRenderer.cpp)
add_library(LightGame::RendererNull ALIAS LightGameRendererNull)
set_target_properties(LightGameRendererNull PROPERTIES EXPORT_NAME RendererNull)
target_include_directories(LightGameRendererNull PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
	$<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)
target_link_libraries(LightGameRendererNull PUBLIC LightGame::Rendering)
lge_configure_target(LightGameRendererNull)
