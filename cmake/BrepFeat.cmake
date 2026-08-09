include(${CMAKE_CURRENT_LIST_DIR}/BrepKernelIncludes.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/BrepLibType.cmake)

# Document + Part implementations live with feat: Part methods need FeatureTree.
# Keeps brep_core → brep_feat a one-way link (required for SHARED).
add_library(brep_feat ${BREP_LIB_TYPE}
  kernel/src/document.cpp
  kernel/src/part.cpp
  kernel/src/param/parameter.cpp
  kernel/src/feat/feature_tree.cpp
  kernel/src/feat/regenerator.cpp
  kernel/src/feat/box_feature.cpp
  kernel/src/feat/feature_history.cpp
  kernel/src/feat/context.cpp
  kernel/src/feat/sketch_feature.cpp
  kernel/src/feat/extrude_feature.cpp
  kernel/src/sketch/sketch.cpp
  kernel/src/solve2d/solver.cpp
  kernel/src/ops/extrude.cpp
)
brep_kernel_include_dirs(brep_feat)
target_link_libraries(brep_feat PUBLIC brep_core)
set_target_properties(brep_feat PROPERTIES OUTPUT_NAME brep_feat)

if(MSVC)
  target_compile_options(brep_feat PUBLIC /utf-8)
endif()
