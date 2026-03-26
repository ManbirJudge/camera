cmake_minimum_required(VERSION 3.19)

project(camera LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)


set(CMAKE_C_STANDARD 11)   # try to make c98 work
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKe_C_COMPILER gcc)
set(CMAKe_CXX_COMPILER g++)

find_package(FLTK REQUIRED)

set(
    SRC_FILES
    src/core.cpp
    src/main.cpp
    src/settings.cpp
    src/stb_impl.c
    src/utils.cpp
)
add_executable(camera ${SRC_FILES})

target_include_directories(camera PRIVATE ${FLTK_INCLUDE_DIR})
target_include_directories(camera PRIVATE include)
target_link_libraries(camera ${FLTK_LIBRARIES})

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall -Wextra")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -march=native")