add_library(brep_core STATIC
  kernel/src/geometry.cpp
  kernel/src/topology.cpp
  kernel/src/model.cpp
  kernel/src/builder.cpp
  kernel/src/validate.cpp
  kernel/src/dump.cpp
  kernel/src/log.cpp
  kernel/src/mesh.cpp
  kernel/src/guid.cpp
  kernel/src/object_registry.cpp
  kernel/src/part.cpp
  kernel/src/document.cpp
)
target_include_directories(brep_core
  PUBLIC
    ${CMAKE_SOURCE_DIR}/kernel/include
    ${BREP_SPDLOG_DIR}/include   # fmt via spdlog (public log API)
)
target_link_libraries(brep_core PUBLIC Eigen3::Eigen brep_boost_uuid)

if(MSVC)
  target_compile_options(brep_core PUBLIC /utf-8)
endif()
