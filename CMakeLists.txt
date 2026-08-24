cmake_minimum_required(VERSION 3.20)
project(brepkernelstudy LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---------------------------------------------------------------------------
# Thin aggregator (ADR 0004): kernel subproject + optional viewer.
# IDE: open this folder; preset clion-mingw (Qt MinGW)
# ---------------------------------------------------------------------------
get_filename_component(BREP_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}" ABSOLUTE)
include(${BREP_REPO_ROOT}/cmake/BrepLibType.cmake)

add_subdirectory(kernel)

# Repo-wide include boundary lint (needs apps/viewer tree).
enable_testing()
find_package(Python3 COMPONENTS Interpreter)
if(Python3_Interpreter_FOUND)
  add_test(
    NAME include_boundaries
    COMMAND ${Python3_EXECUTABLE}
            ${BREP_REPO_ROOT}/scripts/check_include_boundaries.py
            --root ${BREP_REPO_ROOT}
  )
  set_tests_properties(include_boundaries PROPERTIES LABELS "lint;phase4")
else()
  message(WARNING
    "Python3 not found: include_boundaries ctest will be skipped.")
endif()

# ---------------------------------------------------------------------------
# Optional Viewer
# ---------------------------------------------------------------------------
include(${BREP_REPO_ROOT}/cmake/BrepQt.cmake)

option(BREP_BUILD_VIEWER "Build Qt/Vulkan B-Rep viewer" ON)
if(BREP_BUILD_VIEWER)
  include(${BREP_REPO_ROOT}/cmake/BrepViewerDeps.cmake)

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
      "or -DCMAKE_PREFIX_PATH=C:/Qt6/6.8.3/mingw_64")
  endif()
endif()
