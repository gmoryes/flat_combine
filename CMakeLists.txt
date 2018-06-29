cmake_minimum_required(VERSION 2.8.2)
project(flat_combine)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "-pthread -std=c++0x")

set(CMAKE_CXX_FLAGS_TSAN "-g -fsanitize=thread -fno-sanitize-recover=all"
    CACHE STRING "Compiler flags in tsan build"
    FORCE)

include_directories(include)

set(BUILD_GTEST ON CACHE BOOL "Builds the googletest subproject")
set(BUILD_GMOCK ON CACHE BOOL "Builds the googlemock subproject")
set(gtest_disable_pthreads ON CACHE BOOL "Disable uses of pthreads in gtest.")
add_subdirectory(third-party/googletest-release-1.8.0)

add_subdirectory(test)
add_subdirectory(src)


