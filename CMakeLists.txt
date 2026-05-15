cmake_minimum_required(VERSION 3.16)
project(LibreBio VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
    add_compile_options(-Wall -Wextra -Wpedantic -fno-exceptions)
endif()

set(LIBREBIO_INCLUDE_DIR ${CMAKE_SOURCE_DIR}/include)

add_library(libre_bio_core STATIC
    src/core/sequence.cpp
    src/core/quality_score.cpp
)

target_include_directories(libre_bio_core PUBLIC
    ${LIBREBIO_INCLUDE_DIR}
)

include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG        v3.7.1
)
FetchContent_MakeAvailable(Catch2)

add_executable(sequence_test
    test/core/sequence_test.cpp
)

target_link_libraries(sequence_test PRIVATE
    libre_bio_core
    Catch2::Catch2WithMain
)

add_executable(quality_score_test
    test/core/quality_score_test.cpp
)

target_link_libraries(quality_score_test PRIVATE
    libre_bio_core
    Catch2::Catch2WithMain
)

include(CTest)
include(Catch)
catch_discover_tests(sequence_test)
catch_discover_tests(quality_score_test)

# ============================================================================
# 静态分析工具集成
# ============================================================================

# --- clang-tidy ---
find_program(CLANG_TIDY_EXE NAMES clang-tidy clang-tidy-18 clang-tidy-17 clang-tidy-16)

# 探测 macOS SDK 路径（xcode-select -p 指向的 SDK）
# Homebrew 的 LLVM clang-tidy 不会自动搜索系统头文件目录，必须显式指定 -isysroot
if(APPLE)
    find_program(XCRUN_EXE NAMES xcrun)
    if(XCRUN_EXE)
        execute_process(
            COMMAND ${XCRUN_EXE} --show-sdk-path
            OUTPUT_VARIABLE LIBREBIO_MACOS_SDK_PATH
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
    endif()
endif()

if(CLANG_TIDY_EXE)
    set(CLANG_TIDY_SOURCES
        src/core/sequence.cpp
        src/core/quality_score.cpp
    )

    set(LINT_EXTRA_ARGS "")
    if(DEFINED LIBREBIO_MACOS_SDK_PATH AND NOT LIBREBIO_MACOS_SDK_PATH STREQUAL "")
        list(APPEND LINT_EXTRA_ARGS --extra-arg=-isysroot --extra-arg=${LIBREBIO_MACOS_SDK_PATH})
    endif()

    add_custom_target(lint
        COMMENT "Running clang-tidy (MISRA C++:2023 subset checks)"
        COMMAND ${CLANG_TIDY_EXE}
            -p ${CMAKE_BINARY_DIR}
            --config-file=${CMAKE_SOURCE_DIR}/.clang-tidy
            --header-filter=.*libre_bio.*
            ${LINT_EXTRA_ARGS}
            ${CLANG_TIDY_SOURCES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        USES_TERMINAL
    )

    message(STATUS "clang-tidy found: ${CLANG_TIDY_EXE} — target 'lint' available")
else()
    message(WARNING "clang-tidy not found — target 'lint' disabled. "
                    "Install with: brew install llvm (macOS) / apt install clang-tidy (Linux)")
endif()

# --- cppcheck ---
find_program(CPPCHECK_EXE NAMES cppcheck)

if(CPPCHECK_EXE)
    set(CPPCHECK_SOURCES
        src/core/sequence.cpp
        src/core/quality_score.cpp
    )

    add_custom_target(analyze
        COMMENT "Running cppcheck static analysis"
        COMMAND ${CPPCHECK_EXE}
            --enable=warning,performance,portability
            --inconclusive
            --std=c++17
            --language=c++
            --suppress=missingIncludeSystem
            --suppress=unusedFunction
            --suppressions-list=${CMAKE_SOURCE_DIR}/.cppcheck.suppress
            --error-exitcode=1
            -I ${CMAKE_SOURCE_DIR}/include
            ${CPPCHECK_SOURCES}
        WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
        USES_TERMINAL
    )

    message(STATUS "cppcheck found: ${CPPCHECK_EXE} — target 'analyze' available")
else()
    message(WARNING "cppcheck not found — target 'analyze' disabled. "
                    "Install with: brew install cppcheck (macOS) / apt install cppcheck (Linux)")
endif()

# --- 组合目标: 执行全部静态分析 ---
add_custom_target(check_all
    COMMENT "Running all static analysis tools"
    DEPENDS lint analyze
)
