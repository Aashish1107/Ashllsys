cmake_minimum_required(VERSION 3.20)
project(Ashllsys LANGUAGES CXX)

# Default to an optimized build; an unoptimized benchmark is meaningless
if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_executable(llsys main.cpp)
target_compile_features(llsys PRIVATE cxx_std_20)
target_compile_options(llsys PRIVATE -march=native -Wall -Wextra)