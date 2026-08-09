include(${CMAKE_CURRENT_LIST_DIR}/BrepKernelIncludes.cmake)

add_library(brep_asm STATIC
  kernel/src/asm/assembly.cpp
)
brep_kernel_include_dirs(brep_asm)
target_link_libraries(brep_asm PUBLIC brep_core)

if(MSVC)
  target_compile_options(brep_asm PUBLIC /utf-8)
endif()
