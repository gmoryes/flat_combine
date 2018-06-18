# build service
set(SOURCE_FILES
        ArrayUsageExample.cpp
        )

add_executable(runArrayUsageExample ${SOURCE_FILES})
target_link_libraries(runArrayUsageExample gtest gtest_main multithread_array)

add_test(runArrayUsageExample runArrayUsageExample)