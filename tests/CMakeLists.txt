include(FetchContent)

# Fetch GoogleTest
FetchContent_Declare(
  googletest
  URL https://github.com/google/googletest/archive/refs/heads/main.zip
)
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

# Define test executable
add_executable(engine_tests
    test_portfolio.cpp
    test_event_queue.cpp
    test_engine_base.cpp
    test_execution_engine_base.cpp
)

target_link_libraries(engine_tests
    PRIVATE
        quant_engine
        GTest::gtest_main
)

target_compile_options(gtest PRIVATE -Wno-null-dereference)
target_compile_options(gtest_main PRIVATE -Wno-null-dereference)
# Relax warnings just for test files
target_compile_options(engine_tests PRIVATE
    -Wall -Wextra
    -Wno-missing-field-initializers
    -Wno-conversion
)

# Register tests
include(GoogleTest)
gtest_discover_tests(engine_tests)
