include(${CMAKE_CURRENT_LIST_DIR}/BrepKernelIncludes.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/BrepLibType.cmake)

add_library(brep_io ${BREP_LIB_TYPE}
  kernel/src/io/xl_document.cpp
  kernel/src/io/bks_cache.cpp
)
brep_kernel_include_dirs(brep_io)
target_link_libraries(brep_io PUBLIC brep_feat brep_asm)
set_target_properties(brep_io PROPERTIES OUTPUT_NAME brep_io)

if(MSVC)
  target_compile_options(brep_io PUBLIC /utf-8)
endif()
