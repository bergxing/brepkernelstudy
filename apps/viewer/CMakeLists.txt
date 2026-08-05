set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Widgets Gui)

# Prefer checked-in SPIR-V (apps/viewer/spv). Optional glslc rebuild:
#   cmake -DBREP_COMPILE_SHADERS=ON ...
option(BREP_COMPILE_SHADERS "Compile GLSL with glslc instead of prebuilt SPIR-V" OFF)
find_program(GLSLC_EXECUTABLE
  NAMES glslc glslc.exe
  HINTS ENV VULKAN_SDK
  PATH_SUFFIXES Bin bin
)

set(VIEWER_SHADERS
  shaders/mesh.vert
  shaders/mesh.frag
  shaders/line.vert
  shaders/line.frag
)

set(VIEWER_SPV_DIR ${CMAKE_CURRENT_BINARY_DIR}/spv)
file(MAKE_DIRECTORY ${VIEWER_SPV_DIR})

set(VIEWER_SPV_OUTPUTS)
foreach(shader IN LISTS VIEWER_SHADERS)
  get_filename_component(shader_name ${shader} NAME)
  set(spv_out ${VIEWER_SPV_DIR}/${shader_name}.spv)
  set(prebuilt ${CMAKE_CURRENT_SOURCE_DIR}/spv/${shader_name}.spv)
  if(BREP_COMPILE_SHADERS AND GLSLC_EXECUTABLE)
    add_custom_command(
      OUTPUT ${spv_out}
      COMMAND ${GLSLC_EXECUTABLE} -O -o ${spv_out}
              ${CMAKE_CURRENT_SOURCE_DIR}/${shader}
      DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/${shader}
      COMMENT "Compiling ${shader}"
      VERBATIM
    )
  else()
    if(NOT EXISTS ${prebuilt})
      message(FATAL_ERROR
        "Missing prebuilt shader: ${prebuilt}\n"
        "Run: python scripts/gen_spv.py\n"
        "Or enable -DBREP_COMPILE_SHADERS=ON with a working glslc")
    endif()
    add_custom_command(
      OUTPUT ${spv_out}
      COMMAND ${CMAKE_COMMAND} -E copy ${prebuilt} ${spv_out}
      DEPENDS ${prebuilt}
      COMMENT "Copying prebuilt ${shader_name}.spv"
      VERBATIM
    )
  endif()
  list(APPEND VIEWER_SPV_OUTPUTS ${spv_out})
endforeach()

add_custom_target(brep_viewer_shaders DEPENDS ${VIEWER_SPV_OUTPUTS})

qt_add_executable(brep_viewer
  main.cpp
  main_window.cpp
  vulkan_window.cpp
  vulkan_renderer.cpp
  ${VIEWER_SPV_OUTPUTS}
)
add_dependencies(brep_viewer brep_viewer_shaders)

target_include_directories(brep_viewer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(brep_viewer PRIVATE
  BREP_VIEWER_SPV_DIR="${VIEWER_SPV_DIR}"
)
# Qt Gui links the platform Vulkan loader; headers come from the Vulkan-Headers submodule.
target_link_libraries(brep_viewer PRIVATE
  brep
  Qt6::Widgets
  Qt6::Gui
  Vulkan::Headers
)

set_target_properties(brep_viewer PROPERTIES
  WIN32_EXECUTABLE FALSE
)
