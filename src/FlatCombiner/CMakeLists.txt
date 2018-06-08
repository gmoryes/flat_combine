# build service
set(SOURCE_FILES
    FlatCombiner.h
    FlatCombiner.cpp

    ThreadLocal.h
    )

add_library(flat_combiner ${SOURCE_FILES})