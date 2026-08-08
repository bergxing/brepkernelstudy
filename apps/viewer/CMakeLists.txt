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
  shaders/axis.vert
  shaders/axis.frag
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
  home_window.cpp
  document.cpp
  splash_screen.cpp
  view_cube.cpp
  view_panel.cpp
  property_panel.cpp
  vulkan_window.cpp
  vulkan_renderer.cpp
  ecs/world.cpp
  ecs/systems.cpp
  io/dxf_export.cpp
  commands/command_registry.cpp
  commands/builtin_commands.cpp
  commands/command_manager.cpp
  commands/document_history.cpp
  commands/picking.cpp
  commands/command_palette.cpp
  commands/tools/create_box_tool.cpp
  ${VIEWER_SPV_OUTPUTS}
)
add_dependencies(brep_viewer brep_viewer_shaders)

set(VIEWER_ASSETS_DIR ${CMAKE_CURRENT_BINARY_DIR}/assets)
file(MAKE_DIRECTORY ${VIEWER_ASSETS_DIR})
add_custom_command(TARGET brep_viewer POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_CURRENT_SOURCE_DIR}/assets
          ${VIEWER_ASSETS_DIR}
  COMMAND ${CMAKE_COMMAND} -E copy_directory
          ${CMAKE_CURRENT_SOURCE_DIR}/assets
          $<TARGET_FILE_DIR:brep_viewer>/assets
  COMMENT "Copy viewer material assets"
  VERBATIM
)

target_include_directories(brep_viewer PRIVATE ${CMAKE_CURRENT_SOURCE_DIR})
target_compile_definitions(brep_viewer PRIVATE
  BREP_VIEWER_SPV_DIR="${VIEWER_SPV_DIR}"
  BREP_VIEWER_ASSETS_DIR="${VIEWER_ASSETS_DIR}"
)
# Qt Gui links the platform Vulkan loader; headers come from the Vulkan-Headers submodule.
target_link_libraries(brep_viewer PRIVATE
  brep
  EnTT::EnTT
  Qt6::Widgets
  Qt6::Gui
  Vulkan::Headers
)

set_target_properties(brep_viewer PROPERTIES
  WIN32_EXECUTABLE FALSE
)

# ---------------------------------------------------------------------------
# Windows: deploy Qt (+ MinGW runtime) next to the exe so CLion Run works
# without manually editing PATH. 0xC0000135 = missing DLL.
# ---------------------------------------------------------------------------
if(WIN32)
  # CLion's bundled MinGW is ABI-incompatible with Qt's mingw_64 kit.
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" AND
     CMAKE_CXX_COMPILER MATCHES "CLion|JetBrains")
    message(FATAL_ERROR
      "Refusing to build brep_viewer with CLion's bundled MinGW:\n"
      "  ${CMAKE_CXX_COMPILER}\n"
      "Qt mingw_64 requires Qt's MinGW, e.g.:\n"
      "  C:/Qt6/Tools/mingw1310_64/bin/g++.exe\n\n"
      "Fix in CLion:\n"
      "  1. Settings → Build, Execution, Deployment → CMake\n"
      "  2. Delete or disable the profile named 'Debug'\n"
      "  3. Add/enable profile from preset: clion-mingw\n"
      "  4. Generation path should be cmake-build-mingw-debug\n"
      "  5. Reload CMake, then build brep_viewer")
  endif()

  get_filename_component(_qt_bin_dir "${Qt6_DIR}/../../../bin" ABSOLUTE)
  find_program(WINDEPLOYQT_EXECUTABLE
    NAMES windeployqt windeployqt.exe
    HINTS "${_qt_bin_dir}"
    NO_DEFAULT_PATH
  )
  if(WINDEPLOYQT_EXECUTABLE)
    add_custom_command(TARGET brep_viewer POST_BUILD
      COMMAND "${WINDEPLOYQT_EXECUTABLE}"
              --no-translations
              --no-system-d3d-compiler
              "$<TARGET_FILE:brep_viewer>"
      COMMENT "windeployqt brep_viewer"
      VERBATIM
    )
  else()
    message(WARNING "windeployqt not found under ${_qt_bin_dir}")
  endif()

  # MinGW runtime DLLs (same toolchain that compiled the binary).
  if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
    get_filename_component(_mingw_bin_dir "${CMAKE_CXX_COMPILER}" DIRECTORY)
    foreach(_dll IN ITEMS libgcc_s_seh-1.dll libstdc++-6.dll libwinpthread-1.dll)
      if(EXISTS "${_mingw_bin_dir}/${_dll}")
        add_custom_command(TARGET brep_viewer POST_BUILD
          COMMAND "${CMAKE_COMMAND}" -E copy_if_different
                  "${_mingw_bin_dir}/${_dll}"
                  "$<TARGET_FILE_DIR:brep_viewer>/${_dll}"
          COMMENT "Copy ${_dll}"
          VERBATIM
        )
      endif()
    endforeach()
  endif()
endif()
