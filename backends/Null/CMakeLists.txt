add_library(LightGameRendererNull STATIC src/NullRenderer.cpp)
add_library(LightGame::RendererNull ALIAS LightGameRendererNull)
target_include_directories(LightGameRendererNull PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>)
target_link_libraries(LightGameRendererNull PUBLIC LightGame::Rendering)
lge_configure_target(LightGameRendererNull)
