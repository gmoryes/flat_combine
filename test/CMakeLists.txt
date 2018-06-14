# build service
include_directories(${PROJECT_SOURCE_DIR}/src)

add_subdirectory(ThreadLocal)
add_subdirectory(Storage)
add_subdirectory(FlatCombineLogic)
add_subdirectory(GlobalLockStorage)