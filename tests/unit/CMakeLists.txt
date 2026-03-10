cmake_minimum_required(VERSION 3.28)

# Collect all unit test files
file(GLOB_RECURSE UNIT_TEST_SOURCES "*.cpp")

# Create unit test executable
add_executable(unit_tests ${UNIT_TEST_SOURCES})

# Link with the main library, test helpers, and Catch2
target_link_libraries(unit_tests PUBLIC
    sudoku_lib
    test_helpers
    Catch2::Catch2WithMain
)

# MSVC: suppress C4834 (discarded [[nodiscard]] result) and C4702 (unreachable code in templates) in tests
if(MSVC)
    target_compile_options(unit_tests PRIVATE /wd4834 /wd4702)
endif()

# Add to test suite
add_test(NAME unit_tests COMMAND unit_tests)

# Set output directory
set_target_properties(unit_tests PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/tests"
)