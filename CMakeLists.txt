CMAKE_MINIMUM_REQUIRED(VERSION 4.3.4)

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_FLAGS "-Werror")
set(CMAKE_CXX_FLAGS_DEBUG "-Wall -g -O0")

project(myplot LANGUAGES CXX)

file(
    GLOB MAIN
    src/*.cpp
    src/core/*.cpp
    src/demo/*.cpp
    src/utility/*.cpp
    src/worker/*.cpp
)
# file(GLOB MAIN src/main.cpp)

find_package(Boost REQUIRED COMPONENTS crc)
add_executable(main ${MAIN})


target_include_directories(main PUBLIC src)
target_link_libraries(main PRIVATE Boost::crc)
