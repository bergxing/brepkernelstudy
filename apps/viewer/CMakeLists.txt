cmake_minimum_required(VERSION 3.20)

# Nested under repo root, or standalone with find_package(Brep) (ADR 0004 phase B).
project(brep_viewer LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

get_filename_component(BREP_VIEWER_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}" ABSOLUTE)
get_filename_component(BREP_REPO_ROOT "${BREP_VIEWER_SOURCE_DIR}/../.." ABSOLUTE)
set(BREP_CMAKE_DIR "${BREP_REPO_ROOT}/cmake")

set(_brep_viewer_standalone FALSE)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
  set(_brep_viewer_standalone TRUE)
endif()

if(_brep_viewer_standalone)
  include(${BREP_CMAKE_DIR}/BrepLibType.cmake)
  include(${BREP_CMAKE_DIR}/BrepQt.cmake)
  include(${BREP_CMAKE_DIR}/BrepViewerDeps.cmake)

  brep_detect_qt_prefix(_brep_qt_prefix)
  if(_brep_qt_prefix)
    list(PREPEND CMAKE_PREFIX_PATH "${_brep_qt_prefix}")
    message(STATUS "Using Qt prefix: ${_brep_qt_prefix}")
  endif()

  find_package(Brep CONFIG REQUIRED)
  message(STATUS "Standalone viewer: using find_package(Brep)")
endif()

if(NOT TARGET brep AND NOT TARGET Brep::brep)
  message(FATAL_ERROR
    "Brep kernel not found. Build from repository root (add_subdirectory), "
    "or configure Viewer standalone with CMAKE_PREFIX_PATH / Brep_DIR "
    "pointing at an installed or build-tree Brep package.")
endif()
if(NOT TARGET brep AND TARGET Brep::brep)
  add_library(brep ALIAS Brep::brep)
endif()

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

# ---------------------------------------------------------------------------
# Shared viewer paths (PUBLIC so all viewer_* libs see the same defines)
# ---------------------------------------------------------------------------
set(VIEWER_I18N_DIR ${CMAKE_CURRENT_SOURCE_DIR}/i18n)
set(VIEWER_ASSETS_DIR ${CMAKE_CURRENT_BINARY_DIR}/assets)
file(MAKE_DIRECTORY ${VIEWER_ASSETS_DIR})

function(brep_viewer_configure_lib target)
  target_include_directories(${target} PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})
  target_compile_definitions(${target} PUBLIC
    BREP_VIEWER_SPV_DIR="${VIEWER_SPV_DIR}"
    BREP_VIEWER_ASSETS_DIR="${VIEWER_ASSETS_DIR}"
    BREP_VIEWER_I18N_DIR="${VIEWER_I18N_DIR}"
  )
endfunction()

# ---------------------------------------------------------------------------
# Viewer libraries (SHARED when BREP_BUILD_SHARED=ON)
#
# DAG (top → bottom):
#   brep_viewer.exe → viewer_ui → viewer_runtime → viewer_adapter → brep_*
#
# scene / commands / render historically had mutual includes; they ship as one
# shared library `viewer_runtime` so DLL linking stays acyclic.
# ---------------------------------------------------------------------------

include(${BREP_CMAKE_DIR}/BrepLibType.cmake)

add_library(viewer_adapter ${BREP_LIB_TYPE}
  adapter/scene_adapter.cpp
  adapter/document_service.cpp
)
brep_viewer_configure_lib(viewer_adapter)
set_target_properties(viewer_adapter PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
target_link_libraries(viewer_adapter PUBLIC brep)

add_library(viewer_runtime ${BREP_LIB_TYPE}
  # former viewer_scene
  ecs/world.cpp
  ecs/systems.cpp
  # former viewer_commands
  commands/command_registry.cpp
  commands/builtin_commands.cpp
  commands/command_manager.cpp
  commands/document_history.cpp
  commands/picking.cpp
  commands/command_palette.cpp
  commands/tools/create_box_tool.cpp
  commands/tools/copy_tool.cpp
  io/dxf_export.cpp
  document.cpp
  # former viewer_render
  vulkan_window.cpp
  vulkan_renderer.cpp
  render/vulkan_buffer.cpp
  render/vulkan_renderer_textures.cpp
  render/vulkan_pipeline.cpp
  render/mesh_uploader.cpp
  render/vulkan_draw.cpp
  select_rect_overlay.cpp
)
brep_viewer_configure_lib(viewer_runtime)
add_dependencies(viewer_runtime brep_viewer_shaders)
target_link_libraries(viewer_runtime PUBLIC
  viewer_adapter
  brep
  EnTT::EnTT
  Qt6::Widgets
  Qt6::Gui
  Vulkan::Headers
)

# Compatibility aliases (source layout / docs still refer to these names).
add_library(viewer_scene ALIAS viewer_runtime)
add_library(viewer_commands ALIAS viewer_runtime)
add_library(viewer_render ALIAS viewer_runtime)

add_library(viewer_ui ${BREP_LIB_TYPE}
  app/viewer_app.cpp
  main_window.cpp
  ui/view_manager.cpp
  ui/cursor_tip.cpp
  ui/context_menu.cpp
  ui/commands_bridge.cpp
  ui/main_window_menus.cpp
  ui/property_dock_setup.cpp
  ui/window_lifecycle.cpp
  ui/view_cube_layout.cpp
  ui/input_router.cpp
  home_window.cpp
  splash_screen.cpp
  view_cube.cpp
  view_mdi_subwindow.cpp
  property_panel.cpp
  i18n/language_manager.cpp
)
brep_viewer_configure_lib(viewer_ui)
target_link_libraries(viewer_ui PUBLIC
  viewer_adapter
  viewer_runtime
  brep
  EnTT::EnTT
  Qt6::Widgets
  Qt6::Gui
)

# Phase 3 adapter unit tests (GoogleTest from root FetchContent).
if(TARGET GTest::gtest_main)
  add_subdirectory(tests)
endif()

# ---------------------------------------------------------------------------
# Thin executable
# ---------------------------------------------------------------------------
qt_add_executable(brep_viewer main.cpp)
target_link_libraries(brep_viewer PRIVATE viewer_ui)

# Thin exe has no Q_OBJECT types; keep AUTOMOC off so moc stays in viewer_ui.
set_target_properties(brep_viewer PROPERTIES
  WIN32_EXECUTABLE FALSE
  AUTOMOC OFF
  AUTOUIC OFF
  AUTORCC OFF
)

# Deploy kernel SHARED libs next to the exe (needed for install / standalone builds).
if(BREP_BUILD_SHARED)
  foreach(_brep_dll IN ITEMS brep_core brep_feat brep_asm brep_io)
    if(TARGET ${_brep_dll})
      set(_brep_dll_tgt ${_brep_dll})
    elseif(TARGET Brep::${_brep_dll})
      set(_brep_dll_tgt Brep::${_brep_dll})
    else()
      continue()
    endif()
    add_custom_command(TARGET brep_viewer POST_BUILD
      COMMAND ${CMAKE_COMMAND} -E copy_if_different
              "$<TARGET_FILE:${_brep_dll_tgt}>"
              "$<TARGET_FILE_DIR:brep_viewer>"
      COMMENT "Deploy $<TARGET_FILE_NAME:${_brep_dll_tgt}>"
      VERBATIM
    )
  endforeach()
endif()

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

# Compile .ts → .qm when lrelease is available; always deploy translations/.
find_program(LRELEASE_EXECUTABLE NAMES lrelease lrelease-qt6 lrelease.exe
  HINTS "${Qt6_DIR}/../../../bin")
set(VIEWER_QM_DIR ${CMAKE_CURRENT_BINARY_DIR}/translations)
file(MAKE_DIRECTORY ${VIEWER_QM_DIR})
if(LRELEASE_EXECUTABLE)
  add_custom_command(
    OUTPUT ${VIEWER_QM_DIR}/xcad_zh_CN.qm
    COMMAND ${LRELEASE_EXECUTABLE}
            ${VIEWER_I18N_DIR}/xcad_zh_CN.ts
            -qm ${VIEWER_QM_DIR}/xcad_zh_CN.qm
    DEPENDS ${VIEWER_I18N_DIR}/xcad_zh_CN.ts
    COMMENT "lrelease xcad_zh_CN.ts"
    VERBATIM
  )
  add_custom_target(brep_viewer_i18n DEPENDS ${VIEWER_QM_DIR}/xcad_zh_CN.qm)
  add_dependencies(brep_viewer brep_viewer_i18n)
else()
  message(WARNING "lrelease not found; copy prebuilt .qm if present")
endif()

add_custom_command(TARGET brep_viewer POST_BUILD
  COMMAND ${CMAKE_COMMAND} -E make_directory
          $<TARGET_FILE_DIR:brep_viewer>/translations
  COMMAND ${CMAKE_COMMAND} -E copy_if_different
          ${VIEWER_QM_DIR}/xcad_zh_CN.qm
          $<TARGET_FILE_DIR:brep_viewer>/translations/xcad_zh_CN.qm
  COMMENT "Deploy viewer translations"
  VERBATIM
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
    # Thin exe only imports viewer_*.dll; scan those for Qt deps.
    add_custom_command(TARGET brep_viewer POST_BUILD
      COMMAND "${WINDEPLOYQT_EXECUTABLE}"
              --no-translations
              --no-system-d3d-compiler
              --dir "$<TARGET_FILE_DIR:brep_viewer>"
              "$<TARGET_FILE:viewer_ui>"
              "$<TARGET_FILE:viewer_runtime>"
      COMMENT "windeployqt viewer shared libs"
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
