add_library(brep_feat STATIC
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
target_link_libraries(brep_feat PUBLIC brep_core)

if(MSVC)
  target_compile_options(brep_feat PUBLIC /utf-8)
endif()
