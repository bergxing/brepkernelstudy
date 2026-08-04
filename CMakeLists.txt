cmake_minimum_required(VERSION 3.20)
project(brep-kernel LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

include(FetchContent)

FetchContent_Declare(
  eigen
  GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
  GIT_TAG 3.4.0
  GIT_SHALLOW TRUE
)
set(EIGEN_BUILD_DOC OFF CACHE BOOL "" FORCE)
set(EIGEN_BUILD_TESTING OFF CACHE BOOL "" FORCE)
set(BUILD_TESTING OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
  spdlog
  GIT_REPOSITORY https://github.com/gabime/spdlog.git
  GIT_TAG v1.14.1
  GIT_SHALLOW TRUE
)

FetchContent_MakeAvailable(eigen)
FetchContent_GetProperties(spdlog)
if(NOT spdlog_POPULATED)
  FetchContent_Populate(spdlog)
endif()

add_library(brep
  src/geometry.cpp
  src/topology.cpp
  src/model.cpp
  src/builder.cpp
  src/validate.cpp
  src/dump.cpp
  src/log.cpp
)
target_include_directories(brep
  PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include
  PRIVATE ${spdlog_SOURCE_DIR}/include
)
target_link_libraries(brep PUBLIC Eigen3::Eigen)

if(MSVC)
  target_compile_options(brep PUBLIC /utf-8)
endif()

add_executable(box_demo examples/box_demo.cpp)
target_link_libraries(box_demo PRIVATE brep)

add_executable(smoke examples/smoke.cpp)
target_link_libraries(smoke PRIVATE brep)

enable_testing()
add_test(NAME box_demo COMMAND box_demo)
add_test(NAME smoke COMMAND smoke)
