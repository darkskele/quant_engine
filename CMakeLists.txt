cmake_minimum_required(VERSION 3.16)
project(quant_engine LANGUAGES CXX)

# Require modern C++
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Default to Debug build if not specified
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Debug CACHE STRING "Build type" FORCE)
endif()

# no optimization, full symbols
set(CMAKE_CXX_FLAGS_DEBUG "-O0 -g")

# Release flags: maximum optimization for GCC
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -march=native -DNDEBUG")

# optimized but keeps debug symbols (for perf profiling)
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O3 -march=native -g -DNDEBUG")

# Enable strict warnings (GCC only)
add_compile_options(
    -Wall
    -Wextra
    -Wpedantic
    -Wshadow
    -Wconversion
    -Wsign-conversion
    -Wcast-align
    -Wnull-dereference
    -Wdouble-promotion
    -Wformat=2
)

# Engine library (commented until we have .cpp files)
# add_library(quant_engine
#     src/portfolio/portfolio_manager.cpp
# )
# 
# target_include_directories(quant_engine
#     PUBLIC
#         ${PROJECT_SOURCE_DIR}/include
# )
# 
# target_link_libraries(quant_engine
#     PUBLIC
#         streamer
# )

# For now, create a header-only interface library
add_library(quant_engine INTERFACE)

target_include_directories(quant_engine
    INTERFACE
        ${PROJECT_SOURCE_DIR}/include
)

find_package(GTest CONFIG REQUIRED) 

# Enable tests
enable_testing()
add_subdirectory(tests)
add_subdirectory(benchmarks)