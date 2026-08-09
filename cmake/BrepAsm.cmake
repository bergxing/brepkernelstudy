add_library(brep_asm STATIC
  kernel/src/asm/assembly.cpp
)
target_link_libraries(brep_asm PUBLIC brep_core)

if(MSVC)
  target_compile_options(brep_asm PUBLIC /utf-8)
endif()
