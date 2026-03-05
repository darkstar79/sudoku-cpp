cmake_minimum_required(VERSION 3.28)

# Collect all benchmark programs
file(GLOB BENCHMARK_SOURCES "*.cpp")

# Create benchmark executables
foreach(BENCHMARK_SRC ${BENCHMARK_SOURCES})
    get_filename_component(BENCHMARK_NAME ${BENCHMARK_SRC} NAME_WE)

    add_executable(${BENCHMARK_NAME} ${BENCHMARK_SRC})

    target_link_libraries(${BENCHMARK_NAME} PUBLIC
        sudoku_lib
        test_helpers
        yaml-cpp::yaml-cpp
    )

    set_target_properties(${BENCHMARK_NAME} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/benchmarks"
    )
endforeach()

# Add convenience target to run all benchmarks
# Note: This assumes puzzle_generation_benchmark exists
add_custom_target(run_benchmarks
    COMMAND ${CMAKE_COMMAND} -E echo "Running puzzle generation benchmarks..."
    COMMAND $<TARGET_FILE:puzzle_generation_benchmark> --config ${CMAKE_CURRENT_SOURCE_DIR}/configs/standard.yaml
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    DEPENDS puzzle_generation_benchmark
    COMMENT "Running all benchmarks with standard configuration"
)
