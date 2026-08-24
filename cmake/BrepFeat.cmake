include(${CMAKE_CURRENT_LIST_DIR}/BrepKernelIncludes.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/BrepLibType.cmake)

if(NOT BREP_KERNEL_DIR)
  message(FATAL_ERROR "BREP_KERNEL_DIR is not set")
endif()

add_library(brep_feat ${BREP_LIB_TYPE}
  ${BREP_KERNEL_DIR}/src/Document.cpp
  ${BREP_KERNEL_DIR}/src/Part.cpp
  ${BREP_KERNEL_DIR}/src/param/Parameter.cpp
  ${BREP_KERNEL_DIR}/src/feat/FeatureTree.cpp
  ${BREP_KERNEL_DIR}/src/feat/Regenerator.cpp
  ${BREP_KERNEL_DIR}/src/feat/BoxFeature.cpp
  ${BREP_KERNEL_DIR}/src/feat/SphereFeature.cpp
  ${BREP_KERNEL_DIR}/src/feat/BooleanFeature.cpp
  ${BREP_KERNEL_DIR}/src/feat/FeatureHistory.cpp
  ${BREP_KERNEL_DIR}/src/feat/Context.cpp
  ${BREP_KERNEL_DIR}/src/feat/SketchFeature.cpp
  ${BREP_KERNEL_DIR}/src/feat/ExtrudeFeature.cpp
  ${BREP_KERNEL_DIR}/src/sketch/Sketch.cpp
  ${BREP_KERNEL_DIR}/src/solve2d/Solver.cpp
  ${BREP_KERNEL_DIR}/src/ops/Extrude.cpp
)
brep_kernel_include_dirs(brep_feat)
target_link_libraries(brep_feat PUBLIC brep_core)
set_target_properties(brep_feat PROPERTIES OUTPUT_NAME brep_feat)

if(MSVC)
  target_compile_options(brep_feat PUBLIC /utf-8)
endif()
