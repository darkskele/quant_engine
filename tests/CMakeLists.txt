# Define test executable
add_executable(engine_unit_tests
    test_portfolio.cpp
    test_event_queue.cpp
    test_engine_base.cpp
    test_execution_engine_base.cpp
)

target_link_libraries(engine_unit_tests
    PRIVATE
        quant_engine
        GTest::gtest_main
)

# Relax warnings just for test files
target_compile_options(engine_unit_tests PRIVATE
    -Wall -Wextra
    -Wno-missing-field-initializers
    -Wno-conversion
    -Wno-null-dereference
)

# Register tests
include(GoogleTest)
gtest_discover_tests(engine_unit_tests)
