include(${CMAKE_CURRENT_LIST_DIR}/BrepKernelIncludes.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/BrepLibType.cmake)

if(NOT BREP_KERNEL_DIR)
  message(FATAL_ERROR "BREP_KERNEL_DIR is not set")
endif()

add_library(brep_base ${BREP_LIB_TYPE}
  ${BREP_KERNEL_DIR}/src/base/Aspect.cpp
  ${BREP_KERNEL_DIR}/src/base/Guid.cpp
  ${BREP_KERNEL_DIR}/src/base/Log.cpp
)
brep_kernel_include_dirs(brep_base)
target_link_libraries(brep_base PUBLIC brep_eigen brep_boost_uuid brep_spdlog)
set_target_properties(brep_base PROPERTIES OUTPUT_NAME brep_base)

if(MSVC)
  target_compile_options(brep_base PUBLIC /utf-8)
endif()
