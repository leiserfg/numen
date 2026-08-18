cmake_minimum_required(VERSION 3.21)
set(PROJECT_NAME numen)
project(${PROJECT_NAME})
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BUILD_SHARED_LIBS "Build shared libraries" OFF)
option(BUILD_REPL "Build the repl program" ON)
option(BUILD_REPL_CURRENCY_PROVIDER "Build the built-in currency provider (uses api.vicinae.com). Adds a runtime dependency on OpenSSL." ON)

option(BUILD_TESTS "Build the test suite" ON)
option(BUILD_FUZZER "Build the libFuzzer target. Requires clang." OFF)

if (NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
	set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
	set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif()

if (BUILD_FUZZER)
	if (NOT CMAKE_CXX_COMPILER_ID MATCHES "Clang")
		message(FATAL_ERROR "BUILD_FUZZER requires clang (got '${CMAKE_CXX_COMPILER_ID}'). Configure with -DCMAKE_CXX_COMPILER=clang++.")
	endif()

	# global: coverage feedback needs the library instrumented, not just the entry point
	add_compile_options(-fsanitize=fuzzer-no-link,address,undefined -fno-omit-frame-pointer)
	add_link_options(-fsanitize=address,undefined)
endif()

if (CMAKE_BUILD_TYPE STREQUAL "Debug")
	if (MSVC)
		add_compile_options(/fsanitize=address)
	else()
		add_compile_options(-fsanitize=address,undefined -fno-omit-frame-pointer)
		add_link_options(-fsanitize=address,undefined)
	endif()
endif()

set(VENDOR_DIR ${CMAKE_CURRENT_SOURCE_DIR}/vendor)

if (WIN32)
	set(DEFAULT_USE_SYSTEM_CATCH OFF)
else()
	set(DEFAULT_USE_SYSTEM_CATCH ON)
endif()

option(USE_SYSTEM_CATCH "Use system catch2 for testing. Fetch it from GitHub otherwise." ${DEFAULT_USE_SYSTEM_CATCH})

if (WIN32 AND NOT MSVC)
	message(FATAL_ERROR "Windows builds are only supported with MSVC (got '${CMAKE_CXX_COMPILER_ID}'). Please set -DCMAKE_CXX_COMPILER=cl.")
endif()

set(NUMEN_SRCS 
	src/numen/numen.cpp
	src/numen/builtin-units.cpp
	src/numen/lexer.cpp
	src/numen/parser.cpp
	src/numen/region-currency.cpp
	src/numen/timezone.cpp
	src/numen/unicode.cpp
	src/numen/unit.cpp
	src/numen/value.cpp
)

set(PUBLIC_HEADERS ./include/numen/numen.hpp ./include/numen/unit.hpp ./include/numen/abstract-currency-provider.hpp)

if (BUILD_SHARED_LIBS)
	add_library(${PROJECT_NAME} SHARED ${NUMEN_SRCS})
else()
	add_library(${PROJECT_NAME} STATIC ${NUMEN_SRCS})
endif()

install(TARGETS ${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME})
install(DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/include/numen/ DESTINATION ${CMAKE_INSTALL_PREFIX}/include/numen)

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
	set(SRCS src/repl/main.cpp)
	set(LINK_LIBS)

	if (BUILD_REPL_CURRENCY_PROVIDER)
		find_package(OpenSSL REQUIRED)
		add_compile_definitions(CPPHTTPLIB_OPENSSL_SUPPORT=1)
		add_compile_definitions(BUILD_CURRENCY_PROVIDER=1)
		list(APPEND SRCS
			src/repl/vicinae-currency-provider.cpp
		)
		list(APPEND LINK_LIBS
			OpenSSL::SSL OpenSSL::Crypto
		)
	endif()

	add_executable(${REPL_TARGET} ${SRCS})
	target_include_directories(${REPL_TARGET} PRIVATE
		${CMAKE_CURRENT_SOURCE_DIR}/include
		${VENDOR_DIR}
	)
	target_link_libraries(${REPL_TARGET} PRIVATE ${PROJECT_NAME} ${LINK_LIBS})
	target_include_directories(${REPL_TARGET} PRIVATE ./include ./repl)
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
		list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)
	endif()

	set(TEST_TARGET numen-tests)
	add_executable(${TEST_TARGET} 
		tests/main.cpp 
		tests/datetime.cpp 
		tests/timezone.cpp 
		tests/parse.cpp
		tests/unit.cpp 
		tests/corpus.cpp 
		tests/currency.cpp 
		tests/value.cpp)
	target_compile_definitions(${TEST_TARGET} PRIVATE CORPUS_DIR="${CMAKE_CURRENT_SOURCE_DIR}/tests/corpus")

	target_link_libraries(${TEST_TARGET} PRIVATE Catch2::Catch2WithMain ${PROJECT_NAME})
	target_include_directories(${TEST_TARGET} PRIVATE ./include ./src/numen)

	include(CTest)
	include(Catch)
	catch_discover_tests(${TEST_TARGET})
endif()

if (BUILD_FUZZER)
	set(FUZZ_TARGET numen-fuzz)
	add_executable(${FUZZ_TARGET} tests/fuzz.cpp)
	target_link_libraries(${FUZZ_TARGET} PRIVATE ${PROJECT_NAME})
	target_include_directories(${FUZZ_TARGET} PRIVATE ./include)

	target_link_options(${FUZZ_TARGET} PRIVATE -fsanitize=fuzzer)
endif()
