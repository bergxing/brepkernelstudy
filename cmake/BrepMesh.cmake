include(${CMAKE_CURRENT_LIST_DIR}/BrepKernelIncludes.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/BrepLibType.cmake)

if(NOT BREP_KERNEL_DIR)
  message(FATAL_ERROR "BREP_KERNEL_DIR is not set")
endif()

add_library(brep_mesh ${BREP_LIB_TYPE}
  ${BREP_KERNEL_DIR}/src/mesh/Mesh.cpp
  ${BREP_KERNEL_DIR}/src/mesh/Cdt.cpp
  ${BREP_KERNEL_DIR}/src/mesh/LoopSample.cpp
)
brep_kernel_include_dirs(brep_mesh)
target_link_libraries(brep_mesh PUBLIC brep_core)
set_target_properties(brep_mesh PROPERTIES OUTPUT_NAME brep_mesh)

if(MSVC)
  target_compile_options(brep_mesh PUBLIC /utf-8)
endif()
