include(${CMAKE_CURRENT_LIST_DIR}/BrepKernelIncludes.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/BrepLibType.cmake)

if(NOT BREP_KERNEL_DIR)
  message(FATAL_ERROR "BREP_KERNEL_DIR is not set (include from kernel/CMakeLists.txt)")
endif()

add_library(brep_core ${BREP_LIB_TYPE}
  ${BREP_KERNEL_DIR}/src/geometry.cpp
  ${BREP_KERNEL_DIR}/src/topology.cpp
  ${BREP_KERNEL_DIR}/src/model.cpp
  ${BREP_KERNEL_DIR}/src/builder.cpp
  ${BREP_KERNEL_DIR}/src/validate.cpp
  ${BREP_KERNEL_DIR}/src/dump.cpp
  ${BREP_KERNEL_DIR}/src/log.cpp
  ${BREP_KERNEL_DIR}/src/mesh.cpp
  ${BREP_KERNEL_DIR}/src/guid.cpp
  ${BREP_KERNEL_DIR}/src/object_registry.cpp
)
brep_kernel_include_dirs(brep_core)
target_link_libraries(brep_core PUBLIC brep_eigen brep_boost_uuid brep_spdlog)
set_target_properties(brep_core PROPERTIES OUTPUT_NAME brep_core)

if(MSVC)
  target_compile_options(brep_core PUBLIC /utf-8)
endif()
