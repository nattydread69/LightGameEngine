# Game rules live in a library so the same simulation runs in the graphical demo and
# in the headless integration test (tests/RobotArenaHeadlessTests.cpp).
add_library(RobotArenaGame STATIC RobotArenaGame.cpp RobotArenaGame.h)
target_include_directories(RobotArenaGame PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
target_link_libraries(RobotArenaGame PUBLIC LightGame::Engine)
lge_configure_target(RobotArenaGame)

if(TARGET LightGame::RendererVulkan)
	add_executable(RobotArena main.cpp)
	target_link_libraries(RobotArena PRIVATE RobotArenaGame LightGame::RendererVulkan)
	lge_configure_target(RobotArena)
endif()
