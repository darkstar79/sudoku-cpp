cmake_minimum_required(VERSION 3.28)

# Collect all integration test files
file(GLOB_RECURSE INTEGRATION_TEST_SOURCES "*.cpp")

# Create integration test executable
add_executable(integration_tests ${INTEGRATION_TEST_SOURCES})

# Link with the main library, test helpers, and Catch2
target_link_libraries(integration_tests PUBLIC
    sudoku_lib
    test_helpers
    Catch2::Catch2WithMain
)

# Add to test suite
add_test(NAME integration_tests COMMAND integration_tests)

# Set output directory
set_target_properties(integration_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/tests"
)