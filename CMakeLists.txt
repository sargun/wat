cmake_minimum_required(VERSION 3.0)

set(CMAKE_BUILD_TYPE RELEASE)

set(CMAKE_C_FLAGS "-march=native -Wall")
set(CMAKE_C_FLAGS_RELEASE "-O3")

SET(CMAKE_C_STANDARD 11)

ADD_EXECUTABLE(bench "bench.c")
TARGET_LINK_LIBRARIES(bench rt)

ADD_EXECUTABLE(bench2 "bench2.c")
TARGET_LINK_LIBRARIES(bench2 rt)

ADD_EXECUTABLE(sy "sy.c")

ADD_EXECUTABLE(rdtscp "rdtscp.c")
