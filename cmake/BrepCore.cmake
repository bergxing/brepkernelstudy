include(${CMAKE_CURRENT_LIST_DIR}/BrepKernelIncludes.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/BrepLibType.cmake)

if(NOT BREP_KERNEL_DIR)
  message(FATAL_ERROR "BREP_KERNEL_DIR is not set (include from kernel/CMakeLists.txt)")
endif()

add_library(brep_core ${BREP_LIB_TYPE}
  ${BREP_KERNEL_DIR}/src/Geometry.cpp
  ${BREP_KERNEL_DIR}/src/Topology.cpp
  ${BREP_KERNEL_DIR}/src/Model.cpp
  ${BREP_KERNEL_DIR}/src/Builder.cpp
  ${BREP_KERNEL_DIR}/src/Validate.cpp
  ${BREP_KERNEL_DIR}/src/Dump.cpp
  ${BREP_KERNEL_DIR}/src/ObjectRegistry.cpp
  ${BREP_KERNEL_DIR}/src/snap/SnapQuery.cpp
  ${BREP_KERNEL_DIR}/src/spatial/FaceBvh.cpp
)
brep_kernel_include_dirs(brep_core)
target_link_libraries(brep_core PUBLIC brep_base)
set_target_properties(brep_core PROPERTIES OUTPUT_NAME brep_core)

if(MSVC)
  target_compile_options(brep_core PUBLIC /utf-8)
endif()
