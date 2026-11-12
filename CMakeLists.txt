cmake_minimum_required(VERSION 3.20)

project(LightGameEngine
	VERSION 0.1.0
	DESCRIPTION "Modular C++20 game engine built on LightVulkanGraphics"
	LANGUAGES CXX)

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

# Only the Vulkan backend needs LightVulkanGraphics; everything else builds and
# tests headlessly.
option(LGE_BUILD_VULKAN_BACKEND "Build the LightVulkanGraphics renderer backend and graphical examples" ON)
option(LGE_BUILD_DESKTOP_BRIDGE "Build the LVG desktop application bridge" ON)
# Examples and tests default to off when the engine is embedded in a game's own build
# with add_subdirectory().
if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
	set(LGE_IS_TOP_LEVEL ON)
else()
	set(LGE_IS_TOP_LEVEL OFF)
endif()
option(LGE_BUILD_EXAMPLES "Build example applications" ${LGE_IS_TOP_LEVEL})
option(LGE_BUILD_TESTS "Build automated tests" ${LGE_IS_TOP_LEVEL})
option(LGE_BUILD_LICENSE_ISSUER "Build the internal offline licence issuing utility" OFF)
option(LGE_WARNINGS_AS_ERRORS "Treat compiler warnings as errors on engine targets" OFF)
option(LGE_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer (GCC/Clang)" OFF)
set(LGE_LIGHTVULKANGRAPHICS_SOURCE_DIR "" CACHE PATH
	"Optional LightVulkanGraphics source checkout, used only when no installed package is found")

if(LGE_IS_TOP_LEVEL AND NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(LightGameEngineTargets)
include(LightGameEngineDependencies)

add_subdirectory(src)
if(LGE_BUILD_LICENSE_ISSUER)
	add_executable(lge-license-tool tools/lge-license-tool.cpp)
	target_link_libraries(lge-license-tool PRIVATE LightGame::Licensing)
	lge_configure_target(lge-license-tool)
endif()
add_subdirectory(backends/Null)

if(LGE_BUILD_DESKTOP_BRIDGE)
	find_package(LightVulkanGraphics 3.2 CONFIG REQUIRED COMPONENTS UI)
	add_subdirectory(backends/Desktop)
endif()

if(LGE_BUILD_VULKAN_BACKEND)
	lge_find_light_vulkan_graphics()
	if(LGE_HAVE_LIGHT_VULKAN_GRAPHICS)
		add_subdirectory(backends/Vulkan)
	else()
		message(WARNING
			"LightVulkanGraphics >= 3.2 was not found; the Vulkan backend and graphical examples are disabled. "
			"Install it, set CMAKE_PREFIX_PATH, or set LGE_LIGHTVULKANGRAPHICS_SOURCE_DIR.")
	endif()
endif()

if(LGE_BUILD_EXAMPLES)
	add_subdirectory(examples)
endif()

if(LGE_BUILD_TESTS)
	enable_testing()
	add_subdirectory(tests)
	if(TARGET GettingStarted)
		add_test(NAME GettingStartedExample COMMAND GettingStarted)
		set_tests_properties(GettingStartedExample PROPERTIES LABELS "headless;example")
	endif()
endif()

# Installable SDK targets. Consumers get only public headers and imported targets;
# application assets and internal licence tools remain outside the engine package.
set(_lge_export_targets
	LightGameCore LightGamePhysics LightGameAI LightGameRendering
	LightGameScene LightGameEngine LightGameLicensing LightGameRendererNull)
if(TARGET LightGameRendererVulkan)
	list(APPEND _lge_export_targets LightGameRendererVulkan)
endif()
if(TARGET LightGameDesktop)
	list(APPEND _lge_export_targets LightGameDesktop)
endif()
if(TARGET LightGameRendererVulkan OR TARGET LightGameDesktop)
	set(LGE_CONFIG_HAS_GRAPHICS TRUE)
else()
	set(LGE_CONFIG_HAS_GRAPHICS FALSE)
endif()
install(TARGETS ${_lge_export_targets}
	EXPORT LightGameEngineTargets
	ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
	LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
	RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY backends/Null/include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
if(TARGET LightGameRendererVulkan)
	install(DIRECTORY backends/Vulkan/include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
endif()
if(TARGET LightGameDesktop)
	install(DIRECTORY backends/Desktop/include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
endif()
install(EXPORT LightGameEngineTargets
	FILE LightGameEngineTargets.cmake
	NAMESPACE LightGame::
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/LightGameEngine)
configure_package_config_file(
	cmake/LightGameEngineConfig.cmake.in
	${CMAKE_CURRENT_BINARY_DIR}/LightGameEngineConfig.cmake
	INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/LightGameEngine)
write_basic_package_version_file(
	${CMAKE_CURRENT_BINARY_DIR}/LightGameEngineConfigVersion.cmake
	VERSION ${PROJECT_VERSION}
	COMPATIBILITY SameMajorVersion)
install(FILES
	${CMAKE_CURRENT_BINARY_DIR}/LightGameEngineConfig.cmake
	${CMAKE_CURRENT_BINARY_DIR}/LightGameEngineConfigVersion.cmake
	DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/LightGameEngine)
