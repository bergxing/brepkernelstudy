include(${CMAKE_CURRENT_LIST_DIR}/BrepKernelIncludes.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/BrepLibType.cmake)

if(NOT BREP_KERNEL_DIR)
  message(FATAL_ERROR "BREP_KERNEL_DIR is not set")
endif()

add_library(brep_asm ${BREP_LIB_TYPE}
  ${BREP_KERNEL_DIR}/src/asm/Assembly.cpp
)
brep_kernel_include_dirs(brep_asm)
target_link_libraries(brep_asm PUBLIC brep_feat)
set_target_properties(brep_asm PROPERTIES OUTPUT_NAME brep_asm)

if(MSVC)
  target_compile_options(brep_asm PUBLIC /utf-8)
endif()
