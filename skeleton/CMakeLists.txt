cmake_minimum_required(VERSION 3.10)
project(SkeletonPass)

add_library(SkeletonPass MODULE
    skeleton.cpp
    cfg.cpp
    loops.cpp
    tools.cpp
    # Possibly other .cpp files if you have any
)

target_compile_features(SkeletonPass PRIVATE cxx_range_for cxx_auto_type)
set_target_properties(SkeletonPass PROPERTIES
    COMPILE_FLAGS "-fno-rtti"
)

