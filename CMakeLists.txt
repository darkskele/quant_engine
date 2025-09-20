cmake_minimum_required(VERSION 3.16)
project(quant_engine LANGUAGES CXX)

# Require modern C++
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Engine library
add_library(quant_engine
    src/events/event_queue.cpp
    src/portfolio/portfolio_manager.cpp
)

# Public headers
target_include_directories(quant_engine
    PUBLIC
        ${PROJECT_SOURCE_DIR}/include
)

# Enable tests
enable_testing()
add_subdirectory(tests)
