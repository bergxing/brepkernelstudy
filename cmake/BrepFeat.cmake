include(${CMAKE_CURRENT_LIST_DIR}/BrepKernelIncludes.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/BrepLibType.cmake)

if(NOT BREP_KERNEL_DIR)
  message(FATAL_ERROR "BREP_KERNEL_DIR is not set")
endif()

add_library(brep_feat ${BREP_LIB_TYPE}
  ${BREP_KERNEL_DIR}/src/document.cpp
  ${BREP_KERNEL_DIR}/src/part.cpp
  ${BREP_KERNEL_DIR}/src/param/parameter.cpp
  ${BREP_KERNEL_DIR}/src/feat/feature_tree.cpp
  ${BREP_KERNEL_DIR}/src/feat/regenerator.cpp
  ${BREP_KERNEL_DIR}/src/feat/box_feature.cpp
  ${BREP_KERNEL_DIR}/src/feat/sphere_feature.cpp
  ${BREP_KERNEL_DIR}/src/feat/boolean_feature.cpp
  ${BREP_KERNEL_DIR}/src/feat/feature_history.cpp
  ${BREP_KERNEL_DIR}/src/feat/context.cpp
  ${BREP_KERNEL_DIR}/src/feat/sketch_feature.cpp
  ${BREP_KERNEL_DIR}/src/feat/extrude_feature.cpp
  ${BREP_KERNEL_DIR}/src/sketch/sketch.cpp
  ${BREP_KERNEL_DIR}/src/solve2d/solver.cpp
  ${BREP_KERNEL_DIR}/src/ops/extrude.cpp
)
brep_kernel_include_dirs(brep_feat)
target_link_libraries(brep_feat PUBLIC brep_core)
set_target_properties(brep_feat PROPERTIES OUTPUT_NAME brep_feat)

if(MSVC)
  target_compile_options(brep_feat PUBLIC /utf-8)
endif()
