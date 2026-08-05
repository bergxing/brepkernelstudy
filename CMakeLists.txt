cmake_minimum_required(VERSION 3.20)
project(brep-kernel LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------------------
# Third-party via git submodules (third_party/*)
#   git submodule update --init --recursive
# Qt is provided by the in-repo vcpkg submodule + vcpkg.json (manifest mode):
#   .\third_party\vcpkg\bootstrap-vcpkg.bat
#   cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=third_party/vcpkg/scripts/buildsystems/vcpkg.cmake
# ---------------------------------------------------------------------------
set(BREP_EIGEN_DIR          ${CMAKE_CURRENT_SOURCE_DIR}/third_party/eigen)
set(BREP_SPDLOG_DIR         ${CMAKE_CURRENT_SOURCE_DIR}/third_party/spdlog)
set(BREP_VULKAN_HEADERS_DIR ${CMAKE_CURRENT_SOURCE_DIR}/third_party/Vulkan-Headers)
set(BREP_VOLK_DIR           ${CMAKE_CURRENT_SOURCE_DIR}/third_party/volk)

foreach(path_var IN ITEMS
    BREP_EIGEN_DIR BREP_SPDLOG_DIR BREP_VULKAN_HEADERS_DIR BREP_VOLK_DIR)
  if(NOT EXISTS "${${path_var}}")
    message(FATAL_ERROR
      "Missing submodule: ${${path_var}}. "
      "Run: git submodule update --init --recursive")
  endif()
endforeach()

if(NOT EXISTS "${BREP_EIGEN_DIR}/Eigen/Core")
  message(FATAL_ERROR "Eigen submodule incomplete at third_party/eigen")
endif()
if(NOT EXISTS "${BREP_SPDLOG_DIR}/include/spdlog/spdlog.h")
  message(FATAL_ERROR "spdlog submodule incomplete at third_party/spdlog")
endif()
if(NOT EXISTS "${BREP_VULKAN_HEADERS_DIR}/include/vulkan/vulkan.h")
  message(FATAL_ERROR "Vulkan-Headers submodule incomplete")
endif()

# Eigen (header-only)
if(NOT TARGET Eigen3::Eigen)
  add_library(Eigen3::Eigen INTERFACE IMPORTED GLOBAL)
  set_target_properties(Eigen3::Eigen PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${BREP_EIGEN_DIR}")
endif()

# Vulkan-Headers (header-only)
if(NOT TARGET Vulkan::Headers)
  add_library(Vulkan::Headers INTERFACE IMPORTED GLOBAL)
  set_target_properties(Vulkan::Headers PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${BREP_VULKAN_HEADERS_DIR}/include")
endif()

# volk meta-loader (vendored Vulkan-Headers; loads vulkan-1.dll at runtime)
set(VOLK_PULL_IN_VULKAN ON CACHE BOOL "" FORCE)
set(VULKAN_HEADERS_INSTALL_DIR "${BREP_VULKAN_HEADERS_DIR}" CACHE PATH "" FORCE)
set(VOLK_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(${BREP_VOLK_DIR} ${CMAKE_BINARY_DIR}/_deps/volk EXCLUDE_FROM_ALL)
if(TARGET volk)
  target_link_libraries(volk PUBLIC Vulkan::Headers)
endif()

add_library(brep
  src/geometry.cpp
  src/topology.cpp
  src/model.cpp
  src/builder.cpp
  src/validate.cpp
  src/dump.cpp
  src/log.cpp
  src/mesh.cpp
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

# ---------------------------------------------------------------------------
# Optional Qt6 + Vulkan viewer (requires vcpkg toolchain + qtbase[vulkan])
# ---------------------------------------------------------------------------
option(BREP_BUILD_VIEWER "Build Qt/Vulkan B-Rep viewer" ON)
if(BREP_BUILD_VIEWER)
  find_package(Qt6 QUIET COMPONENTS Widgets Gui)
  if(Qt6_FOUND)
    message(STATUS "Qt6 found: building brep_viewer")
    add_subdirectory(apps/viewer)
  else()
    message(WARNING
      "BREP_BUILD_VIEWER=ON but Qt6 not found. "
      "Configure with the in-repo vcpkg toolchain:\n"
      "  .\\third_party\\vcpkg\\bootstrap-vcpkg.bat\n"
      "  cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE="
      "${CMAKE_SOURCE_DIR}/third_party/vcpkg/scripts/buildsystems/vcpkg.cmake")
  endif()
endif()
