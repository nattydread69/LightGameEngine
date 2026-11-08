# Game logic for RobotArena is a library so the headless integration test can run
# the same simulation the graphical demo shows.
add_subdirectory(RobotArena)
add_subdirectory(GettingStarted)

if(TARGET LightGame::RendererVulkan)
	add_subdirectory(BasicWindow)
	add_subdirectory(BasicScene)
endif()
