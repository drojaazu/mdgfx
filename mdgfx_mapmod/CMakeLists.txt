include(CheckIncludeFiles)


# define project
cmake_minimum_required (VERSION 3.5)
project (mdmapmod VERSION 1.0.0 LANGUAGES CXX)
include(GNUInstallDirs)

set(PROJECT_CONTACT "Damian R (damian@sudden-desu.net)")
set(PROJECT_WEBSITE "https://github.com/drojaazu")

configure_file("${CMAKE_CURRENT_SOURCE_DIR}/src/project.hpp.cfg" "${CMAKE_CURRENT_SOURCE_DIR}/src/project.hpp")

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_COMPILER_NAMES clang++ g++ icpc c++ cxx)
set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -DDEBUG")

if (NOT EXISTS ${CMAKE_BINARY_DIR}/CMakeCache.txt)
  if (NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE "Release" CACHE STRING "" FORCE)
  endif()
endif()

aux_source_directory("${CMAKE_CURRENT_SOURCE_DIR}/src" SRCFILES)
include_directories("${CMAKE_CURRENT_SOURCE_DIR}/inc")

add_executable(${PROJECT_NAME} ${SRCFILES})

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_17)
target_link_libraries(${PROJECT_NAME} png chrgfx z)
