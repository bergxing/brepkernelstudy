# Header-only Boost for Viewer (Hypodermic: signals2, range, algorithm).
# Kernel continues to use vendored brep_boost_uuid only (BrepThirdPartyKernel.cmake).
# Expects BREP_REPO_ROOT.

if(NOT BREP_REPO_ROOT)
  message(FATAL_ERROR "BREP_REPO_ROOT is not set")
endif()

set(BREP_BOOST_HEADERS_DIR "${BREP_REPO_ROOT}/third_party/boost" CACHE PATH
    "Boost include root (directory containing boost/ subfolder)")

function(_brep_boost_headers_ready dir out_var)
  if(EXISTS "${dir}/boost/range/adaptor/reversed.hpp"
     AND EXISTS "${dir}/boost/signals2.hpp"
     AND EXISTS "${dir}/boost/algorithm/string.hpp")
    set(${out_var} TRUE PARENT_SCOPE)
  else()
    set(${out_var} FALSE PARENT_SCOPE)
  endif()
endfunction()

if(NOT TARGET brep_boost_headers)
  add_library(brep_boost_headers INTERFACE)
endif()

_brep_boost_headers_ready("${BREP_BOOST_HEADERS_DIR}" _brep_boost_vendored_ready)

if(NOT _brep_boost_vendored_ready)
  foreach(_brep_vcpkg_triplet IN ITEMS x64-mingw-dynamic x64-windows)
    set(_brep_vcpkg_boost_include
        "${BREP_REPO_ROOT}/third_party/vcpkg/installed/${_brep_vcpkg_triplet}/include")
    _brep_boost_headers_ready("${_brep_vcpkg_boost_include}" _brep_vcpkg_boost_ready)
    if(_brep_vcpkg_boost_ready)
      set(BREP_BOOST_HEADERS_DIR "${_brep_vcpkg_boost_include}" CACHE PATH "" FORCE)
      set(_brep_boost_vendored_ready TRUE)
      break()
    endif()
  endforeach()
endif()

if(_brep_boost_vendored_ready)
  target_include_directories(brep_boost_headers INTERFACE
    $<BUILD_INTERFACE:${BREP_BOOST_HEADERS_DIR}>
  )
  message(STATUS "Boost headers (Hypodermic): ${BREP_BOOST_HEADERS_DIR}")
else()
  find_package(Boost 1.70 QUIET)
  if(Boost_FOUND)
    if(TARGET Boost::headers)
      target_link_libraries(brep_boost_headers INTERFACE Boost::headers)
    elseif(TARGET Boost::boost)
      target_link_libraries(brep_boost_headers INTERFACE Boost::boost)
    else()
      target_include_directories(brep_boost_headers INTERFACE ${Boost_INCLUDE_DIRS})
    endif()
    message(STATUS "Boost headers (Hypodermic): ${Boost_INCLUDE_DIRS}")
  else()
    message(FATAL_ERROR
      "Boost headers required for Hypodermic (signals2, range, algorithm).\n"
      "Option A — vendored (recommended):\n"
      "  Extract Boost >= 1.70 to third_party/boost so that\n"
      "  third_party/boost/boost/range/adaptor/reversed.hpp exists.\n"
      "  Example (PowerShell, from repo root):\n"
      "    curl -L -o boost.tar.xz https://github.com/boostorg/boost/releases/download/boost-1.84.0/boost-1.84.0.tar.xz\n"
      "    tar -xf boost.tar.xz\n"
      "    move boost-1.84.0 third_party/boost\n"
      "Option B — vcpkg:\n"
      "  third_party/vcpkg/vcpkg install boost-signals2 boost-range boost-algorithm:x64-windows\n"
      "Option C — system Boost:\n"
      "  Install Boost >= 1.70 and ensure find_package(Boost) succeeds,\n"
      "  or set -DBREP_BOOST_HEADERS_DIR=<path-to-boost-include-root>")
  endif()
endif()
