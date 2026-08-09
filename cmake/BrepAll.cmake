include(${CMAKE_CURRENT_LIST_DIR}/BrepCore.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/BrepFeat.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/BrepIO.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/BrepAsm.cmake)

# INTERFACE aggregate — compatible with existing target_link_libraries(... brep)
# Dependency DAG (SHARED-safe):
#   core ← feat ← asm
#              ↖ io (also uses asm MateSolver)
add_library(brep INTERFACE)
target_link_libraries(brep INTERFACE brep_io brep_asm brep_feat brep_core)

# Namespaced aliases for in-tree consumers (exported package uses Brep:: via NAMESPACE).
if(NOT TARGET Brep::brep)
  add_library(Brep::brep ALIAS brep)
endif()
if(NOT TARGET Brep::brep_core)
  add_library(Brep::brep_core ALIAS brep_core)
endif()
if(NOT TARGET Brep::brep_feat)
  add_library(Brep::brep_feat ALIAS brep_feat)
endif()
if(NOT TARGET Brep::brep_io)
  add_library(Brep::brep_io ALIAS brep_io)
endif()
if(NOT TARGET Brep::brep_asm)
  add_library(Brep::brep_asm ALIAS brep_asm)
endif()
