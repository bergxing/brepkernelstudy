# Expects BREP_KERNEL_DIR (absolute path to kernel/) before use.
function(brep_kernel_include_dirs target)
  if(NOT BREP_KERNEL_DIR)
    message(FATAL_ERROR "BREP_KERNEL_DIR is not set")
  endif()
  target_include_directories(${target}
    PUBLIC
      $<BUILD_INTERFACE:${BREP_KERNEL_DIR}/include>
      $<INSTALL_INTERFACE:include>
    PRIVATE
      $<BUILD_INTERFACE:${BREP_KERNEL_DIR}/internal>
  )
endfunction()
