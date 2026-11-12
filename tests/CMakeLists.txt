# Headless unit tests: no GPU, window or Vulkan installation required.
function(lge_add_test_suite name)
	add_executable(${name} TestMain.cpp ${ARGN})
	target_include_directories(${name} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
	target_link_libraries(${name} PRIVATE LightGame::Engine LightGame::RendererNull)
	lge_configure_target(${name})
	add_test(NAME ${name} COMMAND ${name})
	set_tests_properties(${name} PROPERTIES LABELS "unit;headless")
endfunction()

lge_add_test_suite(CoreTests CoreTests.cpp)
lge_add_test_suite(EcsTests EcsTests.cpp)
lge_add_test_suite(InputTests InputTests.cpp)
lge_add_test_suite(PhysicsTests PhysicsTests.cpp)
lge_add_test_suite(SceneTests SceneTests.cpp)
lge_add_test_suite(RenderingTests RenderingTests.cpp)
lge_add_test_suite(EngineTests EngineTests.cpp)
lge_add_test_suite(LicensingTests LicensingTests.cpp)
target_link_libraries(LicensingTests PRIVATE LightGame::Licensing)

if(TARGET LightGame::Desktop)
	add_executable(DesktopBridgeTests DesktopBridgeTests.cpp)
	target_link_libraries(DesktopBridgeTests PRIVATE LightGame::Desktop)
	lge_configure_target(DesktopBridgeTests)
	add_test(NAME DesktopBridgeTests COMMAND DesktopBridgeTests)
	set_tests_properties(DesktopBridgeTests PROPERTIES LABELS "headless;integration")
	add_executable(FeedbackConfigTests FeedbackConfigTests.cpp)
	target_link_libraries(FeedbackConfigTests PRIVATE LightGame::Desktop)
	lge_configure_target(FeedbackConfigTests)
	add_test(NAME FeedbackConfigTests COMMAND FeedbackConfigTests)
	set_tests_properties(FeedbackConfigTests PROPERTIES LABELS "headless;unit")
endif()

# Guards the layering rule: no engine header outside backends/Vulkan may mention Vulkan,
# GLFW or LightVulkanGraphics.
add_test(NAME ArchitectureNoVulkanInEngineHeaders
	COMMAND ${CMAKE_COMMAND}
		-DSCAN_DIRS=${PROJECT_SOURCE_DIR}/include$<SEMICOLON>${PROJECT_SOURCE_DIR}/src$<SEMICOLON>${PROJECT_SOURCE_DIR}/backends/Null$<SEMICOLON>${PROJECT_SOURCE_DIR}/backends/Vulkan/include
		-P ${CMAKE_CURRENT_SOURCE_DIR}/CheckNoVulkan.cmake)
set_tests_properties(ArchitectureNoVulkanInEngineHeaders PROPERTIES LABELS "unit;headless;architecture")

if(TARGET RobotArenaGame)
	lge_add_test_suite(RobotArenaHeadlessTests RobotArenaHeadlessTests.cpp)
	target_link_libraries(RobotArenaHeadlessTests PRIVATE RobotArenaGame)
endif()

# Requires a display and a Vulkan-capable device, so it is labelled separately and
# excluded by the default test preset. Run with: ctest --test-dir build -L vulkan
if(TARGET BasicScene)
	add_test(NAME VulkanSmokeBasicScene COMMAND BasicScene --frames 90 --screenshot ${CMAKE_CURRENT_BINARY_DIR}/BasicScene.png)
	add_test(NAME VulkanSmokeRobotArena COMMAND RobotArena --frames 90 --autopilot --screenshot ${CMAKE_CURRENT_BINARY_DIR}/RobotArena.png)
	add_test(NAME VulkanSmokeBasicWindow COMMAND BasicWindow --frames 60)
	set_tests_properties(VulkanSmokeBasicScene VulkanSmokeRobotArena VulkanSmokeBasicWindow PROPERTIES LABELS "vulkan;integration" TIMEOUT 60)
endif()
