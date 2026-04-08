cmake_minimum_required(VERSION 3.19)

project(paneer LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_C_STANDARD 11)   # try to make c98 work
set(CMAKE_C_STANDARD_REQUIRED ON)

set(CMAKE_C_COMPILER gcc)
set(CMAKE_CXX_COMPILER g++)

find_package(JPEG REQUIRED)
find_package(FLTK REQUIRED)

set(
    SRC_FILES
    src/core.cpp
    src/main.cpp
    src/settings.cpp
    src/utils.cpp
)
add_executable(paneer ${SRC_FILES})

target_include_directories(paneer PRIVATE ${FLTK_INCLUDE_DIR})
target_include_directories(paneer PRIVATE include)
target_link_libraries(paneer PRIVATE ${FLTK_LIBRARIES} JPEG::JPEG)

set(CMAKE_CXX_FLAGS_DEBUG "${CMAKE_CXX_FLAGS_DEBUG} -Wall -Wextra")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -march=native")