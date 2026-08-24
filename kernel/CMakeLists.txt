cmake_minimum_required(VERSION 3.20)
project(brep_kernel VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

get_filename_component(BREP_REPO_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/.." ABSOLUTE)
set(BREP_KERNEL_DIR "${CMAKE_CURRENT_SOURCE_DIR}")
set(BREP_CMAKE_DIR "${BREP_REPO_ROOT}/cmake")

include(${BREP_CMAKE_DIR}/BrepLibType.cmake)
include(${BREP_CMAKE_DIR}/BrepThirdPartyKernel.cmake)
include(${BREP_CMAKE_DIR}/BrepAll.cmake)
include(${BREP_CMAKE_DIR}/BrepInstall.cmake)

# ---------------------------------------------------------------------------
# Examples (owned by kernel subproject; sources live at repo examples/)
# ---------------------------------------------------------------------------
option(BREP_BUILD_EXAMPLES "Build kernel example programs" ON)
if(BREP_BUILD_EXAMPLES)
  set(BREP_EXAMPLES_DIR "${BREP_REPO_ROOT}/examples")
  add_executable(box_demo ${BREP_EXAMPLES_DIR}/BoxDemo.cpp)
  target_link_libraries(box_demo PRIVATE brep)

  add_executable(smoke ${BREP_EXAMPLES_DIR}/smoke.cpp)
  target_link_libraries(smoke PRIVATE brep)

  add_executable(parametric_smoke ${BREP_EXAMPLES_DIR}/ParametricSmoke.cpp)
  target_link_libraries(parametric_smoke PRIVATE brep)

  add_executable(xl_roundtrip ${BREP_EXAMPLES_DIR}/XlRoundtrip.cpp)
  target_link_libraries(xl_roundtrip PRIVATE brep)

  enable_testing()
  add_test(NAME box_demo COMMAND box_demo)
  add_test(NAME smoke COMMAND smoke)
  add_test(NAME parametric_smoke COMMAND parametric_smoke)
  add_test(NAME xl_roundtrip COMMAND xl_roundtrip)
endif()

# ---------------------------------------------------------------------------
# GoogleTest (kernel unit tests)
# ---------------------------------------------------------------------------
option(BREP_BUILD_TESTS "Build GoogleTest unit tests" ON)
if(BREP_BUILD_TESTS)
  include(${BREP_CMAKE_DIR}/BrepGoogleTest.cmake)
  brep_make_googletest_available()
  enable_testing()
  add_subdirectory("${BREP_REPO_ROOT}/tests" "${CMAKE_CURRENT_BINARY_DIR}/repo_tests")
endif()
