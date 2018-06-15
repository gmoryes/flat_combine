# build service
set(SOURCE_FILES
    ThreadLocaTest.cpp
    )

add_executable(runThreadLocalTests ${SOURCE_FILES})
target_link_libraries(runThreadLocalTests gtest gtest_main)

add_test(runThreadLocalTests runThreadLocalTests)