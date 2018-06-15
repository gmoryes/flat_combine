cmake_minimum_required(VERSION 3.8.2)
project(flat_combine)

set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_FLAGS "-pthread -g")

set(CMAKE_CXX_FLAGS_TSAN "-g -fsanitize=thread -fno-sanitize-recover=all"
    CACHE STRING "Compiler flags in tsan build"
    FORCE)

add_definitions(-DLOG_LEVEL=${LOG_LEVEL})
if (LOG_LEVEL)
    add_definitions(-DLOG=1)
    message("Log is ON")
else()
    add_definitions(-DLOG=0)
    message("Log is OFF")
endif()

include_directories(include)

set(BUILD_GTEST ON CACHE BOOL "Builds the googletest subproject")
set(BUILD_GMOCK ON CACHE BOOL "Builds the googlemock subproject")
set(gtest_disable_pthreads ON CACHE BOOL "Disable uses of pthreads in gtest.")
add_subdirectory(third-party/googletest-release-1.8.0)

add_subdirectory(test)
add_subdirectory(src)

add_executable(main main.cpp)
target_link_libraries(main storage)


