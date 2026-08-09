# Install / export package config for find_package(Brep) (ADR 0004 phase B).
# Include from kernel/CMakeLists.txt after BrepAll.cmake.
#
# Expects: BREP_KERNEL_DIR, BREP_REPO_ROOT, BREP_CMAKE_DIR, PROJECT_VERSION.

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)

set(BREP_INSTALL_CMAKEDIR "${CMAKE_INSTALL_LIBDIR}/cmake/Brep")

set(_brep_install_targets
  brep_eigen
  brep_boost_uuid
  brep_spdlog
  brep_core
  brep_feat
  brep_asm
  brep_io
  brep
)

install(TARGETS ${_brep_install_targets}
  EXPORT BrepTargets
  RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
  LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
  ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR}
  INCLUDES DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

install(DIRECTORY "${BREP_KERNEL_DIR}/include/brep"
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)
install(DIRECTORY "${BREP_KERNEL_DIR}/include/api"
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}
)

# Vendored header-only deps (paths match INSTALL_INTERFACE on INTERFACE libs).
install(DIRECTORY "${BREP_EIGEN_DIR}/Eigen"
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/brep-third_party/eigen
)
install(DIRECTORY "${BREP_BOOST_UUID_DIR}/boost"
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/brep-third_party/boost_uuid
)
install(DIRECTORY "${BREP_SPDLOG_DIR}/include/spdlog"
  DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/brep-third_party/spdlog/include
)

install(EXPORT BrepTargets
  FILE BrepTargets.cmake
  NAMESPACE Brep::
  DESTINATION ${BREP_INSTALL_CMAKEDIR}
)

# Install-tree Config (keep separate from build-tree Config below).
configure_package_config_file(
  "${BREP_CMAKE_DIR}/BrepConfig.cmake.in"
  "${CMAKE_CURRENT_BINARY_DIR}/BrepConfig.cmake.install"
  INSTALL_DESTINATION ${BREP_INSTALL_CMAKEDIR}
)

write_basic_package_version_file(
  "${CMAKE_CURRENT_BINARY_DIR}/BrepConfigVersion.cmake"
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion
)

install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/BrepConfig.cmake.install"
  DESTINATION ${BREP_INSTALL_CMAKEDIR}
  RENAME BrepConfig.cmake
)
install(FILES
  "${CMAKE_CURRENT_BINARY_DIR}/BrepConfigVersion.cmake"
  DESTINATION ${BREP_INSTALL_CMAKEDIR}
)

# Build-tree package: find_package(Brep) via -DBrep_DIR=<kernel-binary-dir>
export(EXPORT BrepTargets
  FILE "${CMAKE_CURRENT_BINARY_DIR}/BrepTargets.cmake"
  NAMESPACE Brep::
)
file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/BrepConfig.cmake"
[[# Generated build-tree package for find_package(Brep)
include("${CMAKE_CURRENT_LIST_DIR}/BrepTargets.cmake")
if(TARGET Brep::brep AND NOT TARGET brep)
  add_library(brep ALIAS Brep::brep)
endif()
]]
)
