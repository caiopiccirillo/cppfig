# Benchmark configuration for CPPFIG library

find_package(benchmark CONFIG REQUIRED)

# Create benchmark executable
add_executable(
    cppfig_benchmark
    benchmark_main.cpp
)

# Link necessary libraries
target_link_libraries(
    cppfig_benchmark
    PRIVATE
    cppfig::cppfig
    cppfig_warnings
    benchmark::benchmark
    benchmark::benchmark_main
)

# Enable optimizations for accurate benchmarking
target_compile_options(cppfig_benchmark PRIVATE
    $<$<CONFIG:Release>:-O3 -DNDEBUG>
    $<$<CONFIG:RelWithDebInfo>:-O3 -g>
)

# Add benchmark as a custom target (not run by default with make/ninja)
add_custom_target(run_benchmarks
    COMMAND cppfig_benchmark
    DEPENDS cppfig_benchmark
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR}
    COMMENT "Running CPPFIG benchmarks"
)
