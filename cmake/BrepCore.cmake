include(${CMAKE_CURRENT_LIST_DIR}/BrepKernelIncludes.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/BrepLibType.cmake)

add_library(brep_core ${BREP_LIB_TYPE}
  kernel/src/geometry.cpp
  kernel/src/topology.cpp
  kernel/src/model.cpp
  kernel/src/builder.cpp
  kernel/src/validate.cpp
  kernel/src/dump.cpp
  kernel/src/log.cpp
  kernel/src/mesh.cpp
  kernel/src/guid.cpp
  kernel/src/object_registry.cpp
)
brep_kernel_include_dirs(brep_core)
target_include_directories(brep_core
  PUBLIC
    ${BREP_SPDLOG_DIR}/include   # fmt via spdlog (public log API)
)
target_link_libraries(brep_core PUBLIC Eigen3::Eigen brep_boost_uuid)
set_target_properties(brep_core PROPERTIES OUTPUT_NAME brep_core)

if(MSVC)
  target_compile_options(brep_core PUBLIC /utf-8)
endif()
