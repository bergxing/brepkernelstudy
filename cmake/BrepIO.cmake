add_library(brep_io STATIC
  kernel/src/io/xl_document.cpp
  kernel/src/io/bks_cache.cpp
)
target_link_libraries(brep_io PUBLIC brep_feat)

if(MSVC)
  target_compile_options(brep_io PUBLIC /utf-8)
endif()
