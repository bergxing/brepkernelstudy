# Apply PUBLIC api include root + PRIVATE internal include root to a brep_* lib.
function(brep_kernel_include_dirs target)
  target_include_directories(${target}
    PUBLIC
      ${CMAKE_SOURCE_DIR}/kernel/include
    PRIVATE
      ${CMAKE_SOURCE_DIR}/kernel/internal
  )
endfunction()
