# External dependency discovery. Nothing here downloads anything: the build must work
# offline. GLM is the only dependency of the headless engine.

find_package(glm CONFIG QUIET)
if(NOT TARGET glm::glm)
	# Older GLM packages export a bare "glm" target.
	if(TARGET glm)
		add_library(glm::glm ALIAS glm)
	else()
		find_path(LGE_GLM_INCLUDE_DIR glm/glm.hpp)
		if(NOT LGE_GLM_INCLUDE_DIR)
			message(FATAL_ERROR
				"GLM was not found. Install it (e.g. 'sudo apt install libglm-dev') "
				"or point CMAKE_PREFIX_PATH / LGE_GLM_INCLUDE_DIR at a GLM checkout.")
		endif()
		add_library(lge_glm INTERFACE)
		target_include_directories(lge_glm INTERFACE "${LGE_GLM_INCLUDE_DIR}")
		add_library(glm::glm ALIAS lge_glm)
	endif()
endif()

# Locates LightVulkanGraphics in the documented order:
#   1. an installed CMake package (find_package, honours CMAKE_PREFIX_PATH);
#   2. a source checkout given by LGE_LIGHTVULKANGRAPHICS_SOURCE_DIR.
# Version 3.2 is the minimum because the backend relies on orthographic projection,
# per-object visibility and emissive colour, which were added in 3.2.
macro(lge_find_light_vulkan_graphics)
	set(LGE_HAVE_LIGHT_VULKAN_GRAPHICS OFF)
	find_package(LightVulkanGraphics 3.2 CONFIG QUIET)
	if(LightVulkanGraphics_FOUND AND TARGET LightVulkanGraphics::LightVulkanGraphics)
		set(LGE_HAVE_LIGHT_VULKAN_GRAPHICS ON)
		message(STATUS "LightGameEngine: using installed LightVulkanGraphics ${LightVulkanGraphics_VERSION} (${LightVulkanGraphics_DIR})")
	elseif(LGE_LIGHTVULKANGRAPHICS_SOURCE_DIR)
		if(NOT EXISTS "${LGE_LIGHTVULKANGRAPHICS_SOURCE_DIR}/CMakeLists.txt")
			message(FATAL_ERROR "LGE_LIGHTVULKANGRAPHICS_SOURCE_DIR does not contain a CMakeLists.txt: ${LGE_LIGHTVULKANGRAPHICS_SOURCE_DIR}")
		endif()
		set(LVG_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
		set(LVG_BUILD_DOXYGEN OFF CACHE BOOL "" FORCE)
		add_subdirectory("${LGE_LIGHTVULKANGRAPHICS_SOURCE_DIR}" "${CMAKE_BINARY_DIR}/_deps/lightVulkanGraphics" EXCLUDE_FROM_ALL)
		if(TARGET LightVulkanGraphics::LightVulkanGraphics)
			set(LGE_HAVE_LIGHT_VULKAN_GRAPHICS ON)
			message(STATUS "LightGameEngine: building LightVulkanGraphics from ${LGE_LIGHTVULKANGRAPHICS_SOURCE_DIR}")
		endif()
	endif()

	if(LGE_HAVE_LIGHT_VULKAN_GRAPHICS)
		# The backend queries keyboard and mouse state from the window LightVulkanGraphics
		# owns, so it needs GLFW's headers; LightVulkanGraphics links GLFW privately.
		find_package(glfw3 CONFIG QUIET)
		if(NOT TARGET glfw)
			message(WARNING "GLFW was not found; the Vulkan backend needs it for input.")
			set(LGE_HAVE_LIGHT_VULKAN_GRAPHICS OFF)
		endif()
	endif()
endmacro()
