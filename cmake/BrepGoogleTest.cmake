# GoogleTest for kernel / viewer unit tests.
# Offline-first: use vendored third_party/googletest (git submodule).
# Expects BREP_REPO_ROOT when called from kernel/CMakeLists.txt.

if(NOT BREP_REPO_ROOT)
  message(FATAL_ERROR "BREP_REPO_ROOT is not set")
endif()

set(BREP_GOOGLETEST_DIR "${BREP_REPO_ROOT}/third_party/googletest" CACHE PATH
    "Local GoogleTest source tree (tag v1.15.2)")

function(brep_make_googletest_available)
  if(TARGET GTest::gtest_main)
    return()
  endif()

  if(EXISTS "${BREP_GOOGLETEST_DIR}/CMakeLists.txt")
    message(STATUS "Using vendored GoogleTest: ${BREP_GOOGLETEST_DIR}")
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(INSTALL_GMOCK OFF CACHE BOOL "" FORCE)
    add_subdirectory(
      "${BREP_GOOGLETEST_DIR}"
      "${CMAKE_BINARY_DIR}/_deps/googletest-build"
      EXCLUDE_FROM_ALL)
    return()
  endif()

  find_package(GTest CONFIG QUIET)
  if(GTest_FOUND)
    message(STATUS "Using system/package GoogleTest")
    return()
  endif()

  message(FATAL_ERROR
    "GoogleTest is required (BREP_BUILD_TESTS=ON) but was not found.\n"
    "\n"
    "Option A — init submodule (recommended):\n"
    "  git submodule update --init third_party/googletest\n"
    "\n"
    "Option B — manual vendoring:\n"
    "  Extract googletest v1.15.2 into:\n"
    "    ${BREP_GOOGLETEST_DIR}\n"
    "  Mirror (if GitHub is unreachable):\n"
    "    https://gitee.com/mirrors/googletest  (tag v1.15.2)\n"
    "\n"
    "Option C — skip tests for now:\n"
    "  cmake -DBREP_BUILD_TESTS=OFF ...")
endfunction()
