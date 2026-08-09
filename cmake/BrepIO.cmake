include(${CMAKE_CURRENT_LIST_DIR}/BrepKernelIncludes.cmake)

add_library(brep_io STATIC
  kernel/src/io/xl_document.cpp
  kernel/src/io/bks_cache.cpp
)
brep_kernel_include_dirs(brep_io)
target_link_libraries(brep_io PUBLIC brep_feat)

if(MSVC)
  target_compile_options(brep_io PUBLIC /utf-8)
endif()
