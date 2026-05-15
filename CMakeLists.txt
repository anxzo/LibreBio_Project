cmake_minimum_required(VERSION 3.16)
project(LibreBio VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

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
