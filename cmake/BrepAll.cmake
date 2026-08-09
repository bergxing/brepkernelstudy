include(${CMAKE_CURRENT_LIST_DIR}/BrepCore.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/BrepFeat.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/BrepIO.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/BrepAsm.cmake)

# INTERFACE aggregate — compatible with existing target_link_libraries(... brep)
add_library(brep INTERFACE)
# part.cpp (brep_core) references brep_feat symbols — use LINK_GROUP for static cycles.
if(CMAKE_VERSION VERSION_GREATER_EQUAL "3.24")
  target_link_libraries(brep INTERFACE
    "$<LINK_GROUP:RESCAN,brep_core,brep_feat,brep_io,brep_asm>")
else()
  target_link_libraries(brep INTERFACE
    brep_io brep_feat brep_asm brep_core brep_feat)
endif()
