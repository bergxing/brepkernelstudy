cmake_minimum_required(VERSION 3.20)
project(brep-kernel LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------------------
# Third-party via git submodules (third_party/*)
#   git submodule update --init --recursive
#
# Qt is NOT vendored (too large). Use a local Qt install, e.g. C:/Qt6.
# IDE: open this folder in CLion and select preset "clion-mingw"
#   (MinGW from Qt — no Visual Studio required).
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
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
    ${BREP_SPDLOG_DIR}/include   # fmt via spdlog (public log API)
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
# Local Qt6 discovery (kit matches the active compiler: MinGW vs MSVC)
# ---------------------------------------------------------------------------
set(BREP_QT_ROOT "C:/Qt6" CACHE PATH "Qt install root (contains <version>/<kit>)")

function(brep_detect_qt_prefix out_var)
  if(DEFINED ENV{CMAKE_PREFIX_PATH})
    foreach(p IN LISTS ENV{CMAKE_PREFIX_PATH})
      if(EXISTS "${p}/lib/cmake/Qt6/Qt6Config.cmake")
        set(${out_var} "${p}" PARENT_SCOPE)
        return()
      endif()
    endforeach()
  endif()

  foreach(p IN LISTS CMAKE_PREFIX_PATH)
    if(EXISTS "${p}/lib/cmake/Qt6/Qt6Config.cmake")
      set(${out_var} "${p}" PARENT_SCOPE)
      return()
    endif()
  endforeach()

  if(EXISTS "${BREP_QT_ROOT}/lib/cmake/Qt6/Qt6Config.cmake")
    set(${out_var} "${BREP_QT_ROOT}" PARENT_SCOPE)
    return()
  endif()

  # Match Qt ABI to the compiler. CLion/MinGW → mingw_64; MSVC → msvc*_64.
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU"
     OR (CMAKE_CXX_COMPILER_ID STREQUAL "Clang" AND NOT MSVC))
    set(_kits mingw_64 msvc2022_64 msvc2019_64)
  else()
    set(_kits msvc2022_64 msvc2019_64 mingw_64)
  endif()

  if(EXISTS "${BREP_QT_ROOT}")
    file(GLOB _qt_versions LIST_DIRECTORIES true "${BREP_QT_ROOT}/*")
    list(SORT _qt_versions ORDER DESCENDING)
    foreach(ver_dir IN LISTS _qt_versions)
      foreach(kit IN LISTS _kits)
        set(candidate "${ver_dir}/${kit}")
        if(EXISTS "${candidate}/lib/cmake/Qt6/Qt6Config.cmake")
          set(${out_var} "${candidate}" PARENT_SCOPE)
          return()
        endif()
      endforeach()
    endforeach()
  endif()

  set(${out_var} "" PARENT_SCOPE)
endfunction()

# ---------------------------------------------------------------------------
# Optional Qt6 + Vulkan viewer (local Qt install)
# ---------------------------------------------------------------------------
option(BREP_BUILD_VIEWER "Build Qt/Vulkan B-Rep viewer" ON)
if(BREP_BUILD_VIEWER)
  brep_detect_qt_prefix(_brep_qt_prefix)
  if(_brep_qt_prefix)
    list(PREPEND CMAKE_PREFIX_PATH "${_brep_qt_prefix}")
    message(STATUS "Using Qt prefix: ${_brep_qt_prefix}")
  endif()

  find_package(Qt6 QUIET COMPONENTS Widgets Gui)
  if(Qt6_FOUND)
    message(STATUS "Qt6 ${Qt6_VERSION} found: building brep_viewer")
    add_subdirectory(apps/viewer)
  else()
    message(WARNING
      "BREP_BUILD_VIEWER=ON but Qt6 not found.\n"
      "Set BREP_QT_ROOT (e.g. -DBREP_QT_ROOT=C:/Qt6),\n"
      "or -DCMAKE_PREFIX_PATH=C:/Qt6/6.7.3/mingw_64  (CLion/MinGW)\n"
      "or -DCMAKE_PREFIX_PATH=C:/Qt6/6.7.3/msvc2022_64 (MSVC)")
  endif()
endif()
