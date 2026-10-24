cmake_minimum_required(VERSION 3.20)
project(Ashllsys LANGUAGES CXX)

# Default to an optimized build; an unoptimized benchmark is meaningless
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
add_compile_options(-march=native -Wall -Wextra)

find_package(Threads REQUIRED)

add_executable(Ashllsys main.cpp)

add_executable(queue_test queue_test.cpp)
target_link_libraries(queue_test PRIVATE Threads::Threads)