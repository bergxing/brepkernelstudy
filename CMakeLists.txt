cmake_minimum_required(VERSION 3.20)
project(brep-kernel LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------------------
# Third-party dependencies via git submodules (third_party/*)
#   git submodule update --init --recursive
# ---------------------------------------------------------------------------
set(BREP_EIGEN_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/eigen)
set(BREP_SPDLOG_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/spdlog)

if(NOT EXISTS "${BREP_EIGEN_DIR}/Eigen/Core")
  message(FATAL_ERROR
    "Eigen submodule missing at third_party/eigen. "
    "Run: git submodule update --init --recursive")
endif()
if(NOT EXISTS "${BREP_SPDLOG_DIR}/include/spdlog/spdlog.h")
  message(FATAL_ERROR
    "spdlog submodule missing at third_party/spdlog. "
    "Run: git submodule update --init --recursive")
endif()

# Eigen: header-only interface target (avoid configuring Eigen's own tests/docs).
if(NOT TARGET Eigen3::Eigen)
  add_library(Eigen3::Eigen INTERFACE IMPORTED GLOBAL)
  set_target_properties(Eigen3::Eigen PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${BREP_EIGEN_DIR}")
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
  PRIVATE ${BREP_SPDLOG_DIR}/include
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
