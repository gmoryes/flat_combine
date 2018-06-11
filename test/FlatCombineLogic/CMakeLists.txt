set(LOGIC_SOURCE_FILES
    FlatCombineLogicTest.cpp
    )

add_executable(runFlatCombineLogicTest ${LOGIC_SOURCE_FILES})
target_link_libraries(runFlatCombineLogicTest gtest gtest_main storage flat_combiner)

add_test(runFlatCombineLogicTest runFlatCombineLogicTest)

set(DELETE_SOURCE_FILES
        FlatCombineDeleteTest.cpp
        )

add_executable(runFlatCombineDeleteTest ${DELETE_SOURCE_FILES})
target_link_libraries(runFlatCombineDeleteTest gtest gtest_main storage flat_combiner)

add_test(runFlatCombineDeleteTest runFlatCombineDeleteTest)