# Kernel-facing third_party (Eigen, spdlog, Boost.Uuid).
# Expects BREP_REPO_ROOT.
if(NOT BREP_REPO_ROOT)
  message(FATAL_ERROR "BREP_REPO_ROOT is not set")
endif()

set(BREP_EIGEN_DIR          "${BREP_REPO_ROOT}/third_party/eigen" CACHE PATH "" FORCE)
set(BREP_SPDLOG_DIR         "${BREP_REPO_ROOT}/third_party/spdlog" CACHE PATH "" FORCE)
set(BREP_BOOST_UUID_DIR     "${BREP_REPO_ROOT}/third_party/boost_uuid" CACHE PATH "" FORCE)

foreach(path_var IN ITEMS BREP_EIGEN_DIR BREP_SPDLOG_DIR BREP_BOOST_UUID_DIR)
  if(NOT EXISTS "${${path_var}}")
    message(FATAL_ERROR
      "Missing dependency: ${${path_var}}. "
      "Run: git submodule update --init --recursive")
  endif()
endforeach()

if(NOT EXISTS "${BREP_BOOST_UUID_DIR}/boost/uuid/uuid.hpp")
  message(FATAL_ERROR "Boost.Uuid headers missing at ${BREP_BOOST_UUID_DIR}/boost/uuid")
endif()
if(NOT EXISTS "${BREP_EIGEN_DIR}/Eigen/Core")
  message(FATAL_ERROR "Eigen submodule incomplete at third_party/eigen")
endif()
if(NOT EXISTS "${BREP_SPDLOG_DIR}/include/spdlog/spdlog.h")
  message(FATAL_ERROR "spdlog submodule incomplete at third_party/spdlog")
endif()

# Exportable INTERFACE deps (INSTALL_INTERFACE paths match cmake/BrepInstall.cmake).
if(NOT TARGET brep_eigen)
  add_library(brep_eigen INTERFACE)
  target_include_directories(brep_eigen INTERFACE
    $<BUILD_INTERFACE:${BREP_EIGEN_DIR}>
    $<INSTALL_INTERFACE:include/brep-third_party/eigen>
  )
endif()

if(NOT TARGET Eigen3::Eigen)
  add_library(Eigen3::Eigen ALIAS brep_eigen)
endif()

if(NOT TARGET brep_boost_uuid)
  add_library(brep_boost_uuid INTERFACE)
  target_include_directories(brep_boost_uuid INTERFACE
    $<BUILD_INTERFACE:${BREP_BOOST_UUID_DIR}>
    $<INSTALL_INTERFACE:include/brep-third_party/boost_uuid>
  )
endif()

if(NOT TARGET brep_spdlog)
  add_library(brep_spdlog INTERFACE)
  target_include_directories(brep_spdlog INTERFACE
    $<BUILD_INTERFACE:${BREP_SPDLOG_DIR}/include>
    $<INSTALL_INTERFACE:include/brep-third_party/spdlog/include>
  )
endif()
