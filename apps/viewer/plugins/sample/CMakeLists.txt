cmake_minimum_required(VERSION 3.20)

add_library(viewer_plugin_sample SHARED
  SamplePluginModule.cpp
)

target_compile_definitions(viewer_plugin_sample PRIVATE BREP_PLUGIN_EXPORTS)
set_target_properties(viewer_plugin_sample PROPERTIES
  OUTPUT_NAME brep_viewer_plugin_sample
  PREFIX ""
)

target_include_directories(viewer_plugin_sample PRIVATE
  ${CMAKE_CURRENT_SOURCE_DIR}/../..
)

target_link_libraries(viewer_plugin_sample PRIVATE
  Hypodermic::Hypodermic
  brep
)

if(WIN32)
  set_target_properties(viewer_plugin_sample PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugins"
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugins"
    ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib/plugins"
  )
else()
  set_target_properties(viewer_plugin_sample PROPERTIES
    LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/plugins"
  )
endif()
