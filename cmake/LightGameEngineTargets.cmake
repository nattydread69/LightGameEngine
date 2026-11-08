# Helpers that apply the engine's compile settings per target instead of globally,
# so consumers embedding the engine with add_subdirectory() are not affected.

function(lge_configure_target target)
	target_compile_features(${target} PUBLIC cxx_std_20)
	set_target_properties(${target} PROPERTIES
		CXX_EXTENSIONS OFF
		POSITION_INDEPENDENT_CODE ON)

	if(MSVC)
		target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
		if(LGE_WARNINGS_AS_ERRORS)
			target_compile_options(${target} PRIVATE /WX)
		endif()
	else()
		target_compile_options(${target} PRIVATE
			-Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
			-Wnon-virtual-dtor -Wold-style-cast -Woverloaded-virtual -Wno-missing-field-initializers)
		if(LGE_WARNINGS_AS_ERRORS)
			target_compile_options(${target} PRIVATE -Werror)
		endif()
		if(LGE_ENABLE_SANITIZERS)
			target_compile_options(${target} PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
			target_link_options(${target} PUBLIC -fsanitize=address,undefined)
		endif()
	endif()
endfunction()

# Declares an engine library whose public headers live under include/.
function(lge_add_library target alias)
	cmake_parse_arguments(ARG "" "" "SOURCES;PUBLIC_DEPS;PRIVATE_DEPS" ${ARGN})
	add_library(${target} STATIC ${ARG_SOURCES})
	add_library(LightGame::${alias} ALIAS ${target})
	target_include_directories(${target} PUBLIC
		$<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>)
	target_link_libraries(${target} PUBLIC ${ARG_PUBLIC_DEPS} PRIVATE ${ARG_PRIVATE_DEPS})
	lge_configure_target(${target})
endfunction()
