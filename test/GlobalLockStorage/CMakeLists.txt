# build service
set(SOURCE_FILES
        GlobalLockStorageLogicTest.cpp
        )

add_executable(runGlobalLockStorageLogicTests ${SOURCE_FILES})
target_link_libraries(runGlobalLockStorageLogicTests gtest gtest_main global_lock_storage logger)

add_test(runGlobalLockStorageLogicTests runGlobalLockStorageLogicTests)

# build service
set(SOURCE_FILES
        GlobalLockStorageLoadTest.cpp
        )

add_executable(runGlobalLockStorageLoadTest ${SOURCE_FILES})
target_link_libraries(runGlobalLockStorageLoadTest gtest gtest_main global_lock_storage logger)

add_test(runGlobalLockStorageLoadTest runGlobalLockStorageLoadTest)