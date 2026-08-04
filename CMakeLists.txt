cmake_minimum_required(VERSION 3.20)
project(brep-kernel LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

add_library(brep
  src/geometry.cpp
  src/topology.cpp
  src/model.cpp
  src/builder.cpp
  src/validate.cpp
  src/dump.cpp
)
target_include_directories(brep PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

add_executable(box_demo examples/box_demo.cpp)
target_link_libraries(box_demo PRIVATE brep)

enable_testing()
add_test(NAME box_demo COMMAND box_demo)
