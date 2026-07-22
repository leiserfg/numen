cmake_minimum_required(VERSION 3.16)
set(PROJECT_NAME abacus)
project(${PROJECT_NAME})
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BUILD_REPL "Build the repl program" ON)

add_library(${PROJECT_NAME}
	src/abacus.cpp
	vendor/tinyexpr/tinyexpr.cpp
	${SRCS}
)
target_include_directories(${PROJECT_NAME} PRIVATE
	${CMAKE_CURRENT_SOURCE_DIR}/include
	${CMAKE_SOURCE_DIR}/vendor
)

target_compile_features(${PROJECT_NAME} PRIVATE cxx_std_23)
target_include_directories(${PROJECT_NAME} PRIVATE ./include/tinyexpr)

if (BUILD_REPL)
	set(REPL_TARGET repl)
	add_executable(${REPL_TARGET} src/repl.cpp)
	target_compile_features(${REPL_TARGET} PRIVATE cxx_std_23)
	target_link_libraries(${REPL_TARGET} PRIVATE ${PROJECT_NAME})
	target_include_directories(${REPL_TARGET} PRIVATE ./include)
endif()

if (BUILD_TESTS)
	set(TEST_TARGET abacus-tests)
	set(FIXTURE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/fixtures)
	find_package(Catch2 3 REQUIRED)
	add_compile_options(-g3)
	add_executable(${TEST_TARGET} tests/main.cpp)
	target_compile_features(${TEST_TARGET} PRIVATE cxx_std_23)
	target_compile_definitions(${TEST_TARGET} PRIVATE
		XDGPP_FIXTURE_DIR="${FIXTURE_DIR}"
	)
	target_link_libraries(${TEST_TARGET} PRIVATE Catch2::Catch2WithMain ${PROJECT_NAME})
	target_include_directories(${TEST_TARGET} PRIVATE ./include)
endif()

#target_link_libraries(${PROJECT_NAME} PRIVATE)
