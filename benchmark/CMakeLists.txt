# Benchmark configuration for CPPFIG library

find_package(benchmark CONFIG REQUIRED)

# Create benchmark executable
add_executable(
    cppfig_benchmark
    benchmark_main.cpp
    benchmark_scale.cpp
)

# The scale benchmarks share the registry generator with the compile-scale test.
target_include_directories(cppfig_benchmark PRIVATE ${PROJECT_SOURCE_DIR}/test/compile_scale)

# Link necessary libraries
target_link_libraries(
    cppfig_benchmark
    PRIVATE
    cppfig::cppfig
    cppfig_warnings
    benchmark::benchmark
    benchmark::benchmark_main
)

# BENCHMARK() names its registrations with __COUNTER__, which Clang 22 reports
# under -Wpedantic as a C2y extension. The macro is Google Benchmark's, not ours.
# Source file options follow the warning flags cppfig_warnings adds, so the
# suppression is not undone by -Wpedantic.
include(CheckCXXCompilerFlag)
check_cxx_compiler_flag(-Wno-c2y-extensions CPPFIG_HAS_WNO_C2Y_EXTENSIONS)
if(CPPFIG_HAS_WNO_C2Y_EXTENSIONS)
    set_source_files_properties(benchmark_main.cpp benchmark_scale.cpp PROPERTIES COMPILE_OPTIONS -Wno-c2y-extensions)
endif()

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
