cmake_minimum_required(VERSION 3.10)
project(rFlash)

set(CMAKE_CXX_STANDARD 23)

set(SRC src/main.cpp)

find_package(SDL2 REQUIRED)

include_directories(${SDL2_INCLUDE_DIRS} /usr/include/SDL2)

add_executable(rFlash ${SRC})
target_link_libraries(rFlash ${SDL2_LIBRARIES})