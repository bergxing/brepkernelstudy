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

  find_package(Qt6 QUIET COMPONENTS Widgets Gui Svg)
  if(Qt6_FOUND)
    message(STATUS "Qt6 ${Qt6_VERSION} found: building brep_viewer")
    # SARibbon (static, no frameless) — before apps/viewer so target exists.
    set(SARIBBON_BUILD_STATIC_LIBS ON CACHE BOOL "" FORCE)
    set(SARIBBON_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SARIBBON_USE_FRAMELESS_LIB OFF CACHE BOOL "" FORCE)
    set(SARIBBON_INSTALL_IN_CURRENT_DIR OFF CACHE BOOL "" FORCE)
    set(BUILD_TESTS OFF CACHE BOOL "" FORCE)
    if(EXISTS "${BREP_REPO_ROOT}/third_party/SARibbon/CMakeLists.txt")
      add_subdirectory(
        ${BREP_REPO_ROOT}/third_party/SARibbon
        ${CMAKE_BINARY_DIR}/_deps/SARibbon
        EXCLUDE_FROM_ALL
      )
      # Vendor Qt6 deprecation noise (e.g. QHoverEvent ctor in SARibbonBar.cpp).
      if(TARGET SARibbonBar)
        if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
          target_compile_options(SARibbonBar PRIVATE -Wno-deprecated-declarations)
        elseif(MSVC)
          target_compile_options(SARibbonBar PRIVATE /wd4996)
        endif()
      endif()
    else()
      message(FATAL_ERROR
        "third_party/SARibbon missing. Run:\n"
        "  git submodule update --init third_party/SARibbon")
    endif()
    add_subdirectory(apps/viewer)
  else()
    message(WARNING
      "BREP_BUILD_VIEWER=ON but Qt6 not found.\n"
      "Set BREP_QT_ROOT (e.g. -DBREP_QT_ROOT=C:/Qt6),\n"
      "or -DCMAKE_PREFIX_PATH=C:/Qt6/6.8.3/mingw_64")
  endif()
endif()
