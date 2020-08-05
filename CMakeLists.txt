# Copyright 2020 Valeri Ochinski
# SPDX-License-Identifier: Apache-2.0

cmake_minimum_required(VERSION 3.8)

set(CMAKE_COLOR_MAKEFILE ON)
set(CMAKE_CXX_STANDARD 17)
LIST(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/sdl2-cmake-modules")

project(celluar-sim
DESCRIPTION "Experimental 2D simulation of evolution"
LANGUAGES CXX)

set(ENABLE_OPENMP OFF CACHE BOOL "Enable usage of OpenMP if available.\
It is known to decrease performance and increase load considerably.")

if(NOT CMAKE_BUILD_TYPE)
	message(STATUS "No build type specified, defaulting to Release")
	set(CMAKE_BUILD_TYPE "Release")
endif()

add_subdirectory(src)
