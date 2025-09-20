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
)

target_link_libraries(engine_tests
    PRIVATE
        quant_engine
        GTest::gtest_main
)

# Register tests
include(GoogleTest)
gtest_discover_tests(engine_tests)
