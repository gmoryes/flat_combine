# build service
set(SOURCE_FILES
    StorageTest.cpp
    )

add_executable(runStorageTests ${SOURCE_FILES})
target_link_libraries(runStorageTests gtest gtest_main storage)

add_test(runStorageTests runStorageTests)