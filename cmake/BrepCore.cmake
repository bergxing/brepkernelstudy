include(${CMAKE_CURRENT_LIST_DIR}/BrepKernelIncludes.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/BrepLibType.cmake)

if(NOT BREP_KERNEL_DIR)
  message(FATAL_ERROR "BREP_KERNEL_DIR is not set (include from kernel/CMakeLists.txt)")
endif()

add_library(brep_core ${BREP_LIB_TYPE}
  ${BREP_KERNEL_DIR}/src/geometry.cpp
  ${BREP_KERNEL_DIR}/src/topology.cpp
  ${BREP_KERNEL_DIR}/src/model.cpp
  ${BREP_KERNEL_DIR}/src/builder.cpp
  ${BREP_KERNEL_DIR}/src/validate.cpp
  ${BREP_KERNEL_DIR}/src/dump.cpp
  ${BREP_KERNEL_DIR}/src/log.cpp
  ${BREP_KERNEL_DIR}/src/mesh.cpp
  ${BREP_KERNEL_DIR}/src/guid.cpp
  ${BREP_KERNEL_DIR}/src/object_registry.cpp
  ${BREP_KERNEL_DIR}/src/mesh/cdt.cpp
  ${BREP_KERNEL_DIR}/src/mesh/loop_sample.cpp
  ${BREP_KERNEL_DIR}/src/snap/snap_query.cpp
  ${BREP_KERNEL_DIR}/src/bool/evaluator_stub.cpp
  ${BREP_KERNEL_DIR}/src/bool/intersect_plane_plane.cpp
  ${BREP_KERNEL_DIR}/src/bool/intersect_plane_sphere.cpp
  ${BREP_KERNEL_DIR}/src/bool/intersect_sphere_sphere.cpp
  ${BREP_KERNEL_DIR}/src/bool/intersect_plane_cylinder.cpp
  ${BREP_KERNEL_DIR}/src/bool/intersect_sphere_cylinder.cpp
  ${BREP_KERNEL_DIR}/src/bool/box_recognize.cpp
  ${BREP_KERNEL_DIR}/src/bool/box_boolean.cpp
  ${BREP_KERNEL_DIR}/src/bool/planar_recognize.cpp
  ${BREP_KERNEL_DIR}/src/bool/planar_boolean.cpp
  ${BREP_KERNEL_DIR}/src/bool/sphere_recognize.cpp
  ${BREP_KERNEL_DIR}/src/bool/sphere_box_boolean.cpp
  ${BREP_KERNEL_DIR}/src/bool/sphere_sphere_boolean.cpp
  ${BREP_KERNEL_DIR}/src/bool/classify.cpp
  ${BREP_KERNEL_DIR}/src/bool/broadphase.cpp
  ${BREP_KERNEL_DIR}/src/spatial/face_bvh.cpp
)
brep_kernel_include_dirs(brep_core)
target_link_libraries(brep_core PUBLIC brep_eigen brep_boost_uuid brep_spdlog)
set_target_properties(brep_core PROPERTIES OUTPUT_NAME brep_core)

if(MSVC)
  target_compile_options(brep_core PUBLIC /utf-8)
endif()
