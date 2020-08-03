# Copyright 2020 Valeri Ochinski
# SPDX-License-Identifier: Apache-2.0
# Please, keep order of find_package/target_include_directories/target_link_libraries/target_compile_definitions
# same to make future editing easier.

# SDL
find_package(SDL2		REQUIRED)
find_package(SDL2_gfx	REQUIRED)

# OpenMP is not critical but heavily recommended
if(ENABLE_OPENMP)
	find_package(OpenMP)
endif(ENABLE_OPENMP)

add_executable(celluar-sim
	# Source files
	Main.cpp
	Cell.cpp
	# Headers
	Global.hpp
	SdlUtils.hpp
	
	Cell.hpp
)

target_link_libraries(celluar-sim
# SDL
	SDL2::Main
	SDL2::GFX
)

# Handle OpenMP

if(ENABLE_OPENMP)
	if(OpenMP_CXX_FOUND)
		message(STATUS "Compiling with OpenMP")
		target_link_libraries(celluar-sim OpenMP::OpenMP_CXX)
		target_compile_definitions(celluar-sim PRIVATE WITH_OPENMP)
	else()
		message(WARNING "OpenMP support is missing in your compiler.\
This might result in worse performance on most systems. Consider changing compiler\
or use -DENABLE_OPENMP=OFF to silence this warning.")
	endif()
else()
	message(STATUS "OpenMP manually disabled")
endif()

target_compile_options(celluar-sim PRIVATE "$<$<AND:$<CONFIG:DEBUG>,$<CXX_COMPILER_ID:Clang>>:-fstandalone-debug>")
