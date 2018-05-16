# build service
set(SOURCE_FILES
    FlatCombineLogicTest.cpp
    )

add_executable(runFlatCombineLogicTest ${SOURCE_FILES})
target_link_libraries(runFlatCombineLogicTest gtest gtest_main storage flat_combiner)

add_test(runFlatCombineLogicTest runFlatCombineLogicTest)