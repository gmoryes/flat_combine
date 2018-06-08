# build service
include_directories(${PROJECT_SOURCE_DIR}/src)
include_directories(${PROJECT_SOURCE_DIR}/include)

add_subdirectory(ThreadLocal)
add_subdirectory(Storage)
add_subdirectory(FlatCombineLogic)