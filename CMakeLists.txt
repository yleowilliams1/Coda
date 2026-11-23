cmake_minimum_required(VERSION 3.21)
project(coda VERSION 0.1.0 LANGUAGES C)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "../")
# Options
option(FETCH_RAYLIB "Download and build raylib if not installed" OFF)
set(RAYLIB_DIR "" CACHE PATH "Path to local raylib install (containing lib/cmake/raylib)")

if(RAYLIB_DIR)
	list(APPEND CMAKE_PREFIX_PATH "${RAYLIB_DIR}")
endif()

find_package(raylib 5.0 QUIET)

if(NOT raylib_FOUND)
	if(FETCH_RAYLIB)
		include(FetchContent)
		FetchContent_Declare(
			raylib
			GIT_REPOSITORY https://github.com/raysan5/raylib.git
			GIT_TAG 6.0
		)
		FetchContent_MakeAvailable(raylib)
	else()
		message(FATAL_ERROR
			"raylib not found.\n"
			"  - Install it, or\n"
			"  - Point to it:   cmake -S . -B build -DRAYLIB_DIR=/path/to/raylib/install\n"
			"  - Or download:   cmake -S . -B build -DFETCH_RAYLIB=ON\n")
	endif()
endif()

set(GILD_SOURCES
	src/util/gild_astar.c
	src/util/gild_basic.c
	src/util/gild_io.c
	src/util/gild_log.c
	src/util/gild_mem.c
	src/util/gild_math.c
	src/util/gild_string.c
	src/util/gild_window.c
)

add_executable(app
	${GILD_SOURCES}
	src/camera.c
	src/cli.c
	src/dialogue.c
	src/editor.c
	src/entity.c
	src/flag.c
	src/inventory.c
	src/main.c
	src/map.c
	src/save.c
	src/stats.c
)

target_include_directories(app PRIVATE
	src
	src/util
)

# Target C11 for _Generic
target_compile_features(app PRIVATE c_std_11)
target_link_libraries(app PRIVATE raylib)

if(MSVC)
	target_compile_options(app PRIVATE /W3 /Zc:preprocessor /utf-8)
	target_compile_definitions(app PRIVATE _CRT_SECURE_NO_WARNINGS)
else()
	# GCC and Clang
	target_compile_options(app PRIVATE -Wall -Wextra -Wshadow)
endif()

if(WIN32 AND NOT MSVC)
	target_compile_definitions(app PRIVATE __USE_MINGW_ANSI_STDIO=1)
endif()
