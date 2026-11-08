add_executable(BasicScene main.cpp)
target_link_libraries(BasicScene PRIVATE LightGame::Engine LightGame::RendererVulkan)
lge_configure_target(BasicScene)
