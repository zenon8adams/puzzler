cmake_minimum_required(VERSION 3.15)
set(APP_NAME puzzler)
project(${APP_NAME})
set(CMAKE_CXX_STANDARD 17)
if( $<CONFIG:Debug>)
    set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -fsanitize=address -ggdb -Wall -Wconversion -Wpedantic -Wshadow -Werror")
else()
set(CMAKE_CXX_FLAGS  "${CMAKE_CXX_FLAGS} -O2 -Wall")
endif()

# Read the version information from the VERSION file
file(STRINGS "${PROJECT_SOURCE_DIR}/VERSION" PACKAGE_VERSION)
string(REGEX REPLACE "([0-9]+)\\.[0-9]+\\.[0-9]+.*" "\\1" CPACK_PACKAGE_VERSION_MAJOR ${PACKAGE_VERSION})
string(REGEX REPLACE "[0-9]+\\.([0-9]+)\\.[0-9]+.*" "\\1" CPACK_PACKAGE_VERSION_MINOR ${PACKAGE_VERSION})
string(REGEX REPLACE "[0-9]+\\.[0-9]+\\.([0-9]+).*" "\\1" CPACK_PACKAGE_VERSION_PATCH ${PACKAGE_VERSION})
string(REGEX REPLACE ".+(-.+)" "\\1" PRE_RELEASE_TAG ${PACKAGE_VERSION})
if(PRE_RELEASE_TAG STREQUAL "")
unset(PRE_RELEASE_TAG)
endif()
set(PROJECT_VERSION "${CPACK_PACKAGE_VERSION_MAJOR}.${CPACK_PACKAGE_VERSION_MINOR}.${CPACK_PACKAGE_VERSION_PATCH}${PRE_RELEASE_TAG}")
add_executable(${APP_NAME} main.cpp
                       detail/option-builder.hpp
                       detail/puzzle-solver.hpp
                       detail/puzzle-simulator.hpp
                       detail/utility.hpp)
target_compile_definitions(${APP_NAME} PUBLIC APP_NAME="${APP_NAME}")

include(GNUInstallDirs)
install(TARGETS ${APP_NAME} CONFIGURATIONS Release RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR} COMPONENT application)
add_subdirectory(packaging)
