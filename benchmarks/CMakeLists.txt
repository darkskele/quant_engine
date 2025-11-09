# Add Google Benchmark
include(FetchContent)

FetchContent_Declare(
    benchmark
    GIT_REPOSITORY https://github.com/google/benchmark.git
    GIT_TAG v1.8.3
)

# Disable benchmark's tests
set(BENCHMARK_ENABLE_TESTING OFF)
set(BENCHMARK_ENABLE_WERROR OFF) 

FetchContent_MakeAvailable(benchmark)

# Benchmark executable
add_executable(engine_benchmarks
    portfolio_benchmark.cpp
)

target_link_libraries(engine_benchmarks
    PRIVATE
    quant_engine
    benchmark::benchmark
)
