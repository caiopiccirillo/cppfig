# Compiles one source that must not compile, and passes only when the compiler
# fails and its output names the expected diagnostic. Checking the text alone
# would pass a compiler that succeeds while printing a matching warning, and
# checking the exit code alone would pass an unrelated error.
#
# Usage: cmake -DCOMPILER=<c++> -DINCLUDE_DIR=<dir> -DSOURCE=<file.cpp>
#              -DOBJECT=<file.o> -DEXPECTED=<regex> -P expect_compile_failure.cmake

foreach(variable COMPILER INCLUDE_DIR SOURCE OBJECT EXPECTED)
    if(NOT DEFINED ${variable})
        message(FATAL_ERROR "expect_compile_failure.cmake needs -D${variable}=...")
    endif()
endforeach()

execute_process(
    COMMAND "${COMPILER}" -std=c++20 "-I${INCLUDE_DIR}" -c "${SOURCE}" -o "${OBJECT}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE output
)

if(result EQUAL 0)
    file(REMOVE "${OBJECT}")
    message(FATAL_ERROR "${SOURCE} compiled, but it must be rejected with a diagnostic matching '${EXPECTED}'.\n${output}")
endif()
if(NOT output MATCHES "${EXPECTED}")
    message(FATAL_ERROR "${SOURCE} failed to compile, but not with a diagnostic matching '${EXPECTED}':\n${output}")
endif()
