add_executable(GettingStarted main.cpp)
target_link_libraries(GettingStarted PRIVATE LightGame::Engine LightGame::RendererNull)
lge_configure_target(GettingStarted)
