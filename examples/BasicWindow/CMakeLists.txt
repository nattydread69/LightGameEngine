add_executable(BasicWindow main.cpp)
target_link_libraries(BasicWindow PRIVATE LightGame::Engine LightGame::RendererVulkan)
lge_configure_target(BasicWindow)
