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
