# build service
set(SOURCE_FILES
    ThreadLocaTest.cpp
    )

add_executable(runThreadLocalTests ${SOURCE_FILES})
target_link_libraries(runThreadLocalTests gtest gtest_main flat_combiner logger)

add_test(runThreadLocalTests runThreadLocalTests)