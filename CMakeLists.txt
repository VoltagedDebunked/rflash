cmake_minimum_required(VERSION 3.10)
project(rFlash)

set(CMAKE_CXX_STANDARD 23)

set(SRC src/main.cpp)

# Help CMake find the SDL2 and SDL2_mixer packages
set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")

find_package(SDL2 REQUIRED)
find_package(SDL2_mixer REQUIRED)

include_directories(${SDL2_INCLUDE_DIRS} ${SDL2_MIXER_INCLUDE_DIRS})

add_executable(rFlash ${SRC})

# Explicitly link against SDL2 and SDL2_mixer
target_link_libraries(rFlash ${SDL2_LIBRARIES} SDL2_mixer)