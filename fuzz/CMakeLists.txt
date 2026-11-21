# Fuzz targets for the parser, the value conversions and the whole load path.
#
# Every target is built two ways:
# - with CPPFIG_BUILD_FUZZERS (Clang only), as a libFuzzer binary. CTest runs
#   each one for CPPFIG_FUZZ_SECONDS on its corpus; run them by hand for longer.
# - with the tests, against replay_main.cpp, which feeds the checked-in corpus
#   through the target once. The corpus is a regression suite that any
#   compiler and every sanitizer build runs.

set(CPPFIG_FUZZ_TARGETS conf_parser_fuzz traits_roundtrip_fuzz load_fuzz)
set(CPPFIG_FUZZ_SECONDS 30 CACHE STRING "Seconds each fuzzer runs under CTest")

foreach(target_name ${CPPFIG_FUZZ_TARGETS})
    set(corpus_dir ${CMAKE_CURRENT_SOURCE_DIR}/corpus/${target_name})

    if(CPPFIG_BUILD_FUZZERS)
        add_executable(${target_name} ${target_name}.cpp)
        target_link_libraries(${target_name} PRIVATE cppfig::cppfig cppfig_warnings)
        target_compile_options(${target_name} PRIVATE -fsanitize=fuzzer)
        target_link_options(${target_name} PRIVATE -fsanitize=fuzzer)

        # New inputs go to the build tree, so the checked-in corpus only
        # changes when someone copies an interesting input into it.
        set(work_dir ${CMAKE_CURRENT_BINARY_DIR}/${target_name}_corpus)
        file(MAKE_DIRECTORY ${work_dir})
        add_test(
            NAME ${target_name}
            COMMAND ${target_name}
                -max_total_time=${CPPFIG_FUZZ_SECONDS}
                -dict=${CMAKE_CURRENT_SOURCE_DIR}/conf.dict
                -artifact_prefix=${CMAKE_CURRENT_BINARY_DIR}/${target_name}-
                -print_final_stats=1
                ${work_dir}
                ${corpus_dir}
        )
        set_tests_properties(${target_name} PROPERTIES LABELS fuzz TIMEOUT 600)
    endif()

    if(CPPFIG_BUILD_TESTS)
        set(replay_name ${target_name}_replay)
        add_executable(${replay_name} ${target_name}.cpp replay_main.cpp)
        target_link_libraries(${replay_name} PRIVATE cppfig::cppfig cppfig_warnings)
        add_test(NAME ${replay_name} COMMAND ${replay_name} ${corpus_dir})
        coverage_register_target(${replay_name})
    endif()
endforeach()
