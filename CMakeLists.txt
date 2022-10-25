cmake_minimum_required(VERSION 3.15)
project(puzzler)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -o2")

# Read the version information from the VERSION file
file(STRINGS "${PROJECT_SOURCE_DIR}/VERSION" PACKAGE_VERSION)
string(REGEX REPLACE "([0-9]+)\\.[0-9]+\\.[0-9]+.*" "\\1" CPACK_PACKAGE_VERSION_MAJOR ${PACKAGE_VERSION})
string(REGEX REPLACE "[0-9]+\\.([0-9]+)\\.[0-9]+.*" "\\1" CPACK_PACKAGE_VERSION_MINOR ${PACKAGE_VERSION})
string(REGEX REPLACE "[0-9]+\\.[0-9]+\\.([0-9]+).*" "\\1" CPACK_PACKAGE_VERSION_PATCH ${PACKAGE_VERSION})

set(PROJECT_VERSION ${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH})

add_executable(puzzler main.cpp
                       puzzle-solver.hpp
                       puzzle-simulator.hpp
                       utility.hpp)
find_program(USER_SHELL NAME bash PATH_SUFFIXES bin)
target_compile_definitions(puzzler PUBLIC USER_SHELL="${USER_SHELL}")