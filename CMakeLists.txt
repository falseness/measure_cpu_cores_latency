cmake_minimum_required(VERSION 3.16)
project(icore_latency LANGUAGES CXX)


set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


if(NOT CMAKE_BUILD_TYPE)
set(CMAKE_BUILD_TYPE Release CACHE STRING "" FORCE)
endif()


add_compile_options(-Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion -Wshadow -O3 -march=native)


add_executable(icore_latency
src/main.cpp
src/tsc.cpp
src/cpu_topology.cpp
src/sys_utils.cpp
src/measure.cpp)


target_include_directories(icore_latency PRIVATE src)


target_link_libraries(icore_latency PRIVATE pthread)