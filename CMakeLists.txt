cmake_minimum_required(VERSION 3.16)
set(PROJECT_NAME abacus)
project(${PROJECT_NAME})
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BUILD_REPL "Build the repl program" ON)
option(BUILD_TESTS "Build the test suite" ON)

set(VENDOR_DIR ${CMAKE_CURRENT_SOURCE_DIR}/vendor)

if (WIN32)
	set(DEFAULT_USE_SYSTEM_CATCH OFF)
else()
	set(DEFAULT_USE_SYSTEM_CATCH ON)
endif()

option(USE_SYSTEM_CATCH "Use system catch2 for testing. Fetch it from GitHub otherwise." ${DEFAULT_USE_SYSTEM_CATCH})

if (WIN32 AND NOT MSVC)
	message(FATAL_ERROR "Windows builds are only supported with MSVC (got '${CMAKE_CXX_COMPILER_ID}'). Use the windows-* presets or set -DCMAKE_CXX_COMPILER=cl.")
endif()

add_library(${PROJECT_NAME}
	src/abacus.cpp
	src/parser.cpp
	src/timezone.cpp
)
target_include_directories(${PROJECT_NAME} PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/include
	${VENDOR_DIR}
)

if (MSVC)
	add_compile_options(/permissive- /Zc:preprocessor /Zc:__cplusplus /bigobj /utf-8 /W3 /wd4996)
	add_compile_definitions(NOMINMAX WIN32_LEAN_AND_MEAN)
endif()

if (BUILD_REPL)
	set(REPL_TARGET repl)
	add_executable(${REPL_TARGET} src/repl.cpp)
	target_compile_features(${REPL_TARGET} PRIVATE cxx_std_23)
	target_link_libraries(${REPL_TARGET} PRIVATE ${PROJECT_NAME})
	target_include_directories(${REPL_TARGET} PRIVATE ./include)
endif()

if (BUILD_TESTS)
	if (USE_SYSTEM_CATCH)
		find_package(Catch2 3 REQUIRED)
	else()
		Include(FetchContent)

		FetchContent_Declare(
		  Catch2
		  GIT_REPOSITORY https://github.com/catchorg/Catch2.git
		  GIT_TAG        v3.8.1
		)
		FetchContent_MakeAvailable(Catch2)
	endif()

	set(TEST_TARGET abacus-tests)
	add_executable(${TEST_TARGET} tests/main.cpp tests/timezone.cpp tests/datetime.cpp)

	if (NOT MSVC)
		add_compile_options(${TEST_TARGET} -g3)
	endif()

	target_link_libraries(${TEST_TARGET} PRIVATE Catch2::Catch2WithMain ${PROJECT_NAME})
	target_include_directories(${TEST_TARGET} PRIVATE ./include ./src)
endif()
