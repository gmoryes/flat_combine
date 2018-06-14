# build service
set(SOURCE_FILES
        GlobalLockStorageTest.cpp
        )

add_executable(runGlobalLockStorageTests ${SOURCE_FILES})
target_link_libraries(runGlobalLockStorageTests gtest gtest_main global_lock_storage logger)

add_test(runGlobalLockStorageTests runGlobalLockStorageTests)