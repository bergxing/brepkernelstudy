# Viewer-only third_party: Vulkan-Headers, volk, EnTT.
# Expects BREP_REPO_ROOT. Safe to include from root or standalone apps/viewer.

if(NOT BREP_REPO_ROOT)
  message(FATAL_ERROR "BREP_REPO_ROOT is not set")
endif()

set(BREP_VULKAN_HEADERS_DIR "${BREP_REPO_ROOT}/third_party/Vulkan-Headers")
set(BREP_VOLK_DIR           "${BREP_REPO_ROOT}/third_party/volk")
set(BREP_ENTT_DIR           "${BREP_REPO_ROOT}/third_party/entt")

foreach(path_var IN ITEMS BREP_VULKAN_HEADERS_DIR BREP_VOLK_DIR BREP_ENTT_DIR)
  if(NOT EXISTS "${${path_var}}")
    message(FATAL_ERROR
      "Missing Viewer dependency: ${${path_var}}. "
      "Run: git submodule update --init --recursive")
  endif()
endforeach()

if(NOT EXISTS "${BREP_ENTT_DIR}/single_include/entt/entt.hpp")
  message(FATAL_ERROR "EnTT submodule incomplete at third_party/entt")
endif()
if(NOT EXISTS "${BREP_VULKAN_HEADERS_DIR}/include/vulkan/vulkan.h")
  message(FATAL_ERROR "Vulkan-Headers submodule incomplete")
endif()

if(NOT TARGET Vulkan::Headers)
  add_library(Vulkan::Headers INTERFACE IMPORTED GLOBAL)
  set_target_properties(Vulkan::Headers PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${BREP_VULKAN_HEADERS_DIR}/include")
endif()
if(NOT TARGET EnTT::EnTT)
  add_library(EnTT::EnTT INTERFACE IMPORTED GLOBAL)
  set_target_properties(EnTT::EnTT PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${BREP_ENTT_DIR}/single_include")
endif()

set(VOLK_PULL_IN_VULKAN ON CACHE BOOL "" FORCE)
set(VULKAN_HEADERS_INSTALL_DIR "${BREP_VULKAN_HEADERS_DIR}" CACHE PATH "" FORCE)
set(VOLK_INSTALL OFF CACHE BOOL "" FORCE)
if(NOT TARGET volk)
  add_subdirectory(${BREP_VOLK_DIR} ${CMAKE_BINARY_DIR}/_deps/volk EXCLUDE_FROM_ALL)
  if(TARGET volk)
    target_link_libraries(volk PUBLIC Vulkan::Headers)
  endif()
endif()
