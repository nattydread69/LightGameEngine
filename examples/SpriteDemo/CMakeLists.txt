add_executable(SpriteDemo main.cpp)
target_link_libraries(SpriteDemo PRIVATE LightGame::Engine LightGame::RendererVulkan)
lge_configure_target(SpriteDemo)
