# Shared vs static library type for brep_* / viewer_* targets.
option(BREP_BUILD_SHARED "Build kernel and viewer libraries as shared (.dll/.so)" ON)

if(BREP_BUILD_SHARED)
  set(BREP_LIB_TYPE SHARED)
  # Helps MSVC; MinGW typically exports all symbols from DLLs by default.
  set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
else()
  set(BREP_LIB_TYPE STATIC)
endif()

# Put .exe and .dll side-by-side so Windows can find shared libs at runtime.
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
