include(${CMAKE_CURRENT_LIST_DIR}/BrepKernelIncludes.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/BrepLibType.cmake)

if(NOT BREP_KERNEL_DIR)
  message(FATAL_ERROR "BREP_KERNEL_DIR is not set (include from kernel/CMakeLists.txt)")
endif()

add_library(brep_core ${BREP_LIB_TYPE}
  ${BREP_KERNEL_DIR}/src/Geometry.cpp
  ${BREP_KERNEL_DIR}/src/Topology.cpp
  ${BREP_KERNEL_DIR}/src/Model.cpp
  ${BREP_KERNEL_DIR}/src/Builder.cpp
  ${BREP_KERNEL_DIR}/src/Validate.cpp
  ${BREP_KERNEL_DIR}/src/Dump.cpp
  ${BREP_KERNEL_DIR}/src/Log.cpp
  ${BREP_KERNEL_DIR}/src/Mesh.cpp
  ${BREP_KERNEL_DIR}/src/Guid.cpp
  ${BREP_KERNEL_DIR}/src/ObjectRegistry.cpp
  ${BREP_KERNEL_DIR}/src/mesh/Cdt.cpp
  ${BREP_KERNEL_DIR}/src/mesh/LoopSample.cpp
  ${BREP_KERNEL_DIR}/src/snap/SnapQuery.cpp
  ${BREP_KERNEL_DIR}/src/bool/EvaluatorStub.cpp
  ${BREP_KERNEL_DIR}/src/bool/CompositeEvaluator.cpp
  ${BREP_KERNEL_DIR}/src/bool/Pipeline.cpp
  ${BREP_KERNEL_DIR}/src/bool/FastPath.cpp
  ${BREP_KERNEL_DIR}/src/bool/IntersectorRegistry.cpp
  ${BREP_KERNEL_DIR}/src/bool/FaceSelector.cpp
  ${BREP_KERNEL_DIR}/src/bool/IntersectPlanePlane.cpp
  ${BREP_KERNEL_DIR}/src/bool/IntersectPlaneSphere.cpp
  ${BREP_KERNEL_DIR}/src/bool/IntersectSphereSphere.cpp
  ${BREP_KERNEL_DIR}/src/bool/IntersectPlaneCylinder.cpp
  ${BREP_KERNEL_DIR}/src/bool/IntersectSphereCylinder.cpp
  ${BREP_KERNEL_DIR}/src/bool/BoxRecognize.cpp
  ${BREP_KERNEL_DIR}/src/bool/BoxBoolean.cpp
  ${BREP_KERNEL_DIR}/src/bool/PlanarRecognize.cpp
  ${BREP_KERNEL_DIR}/src/bool/PlanarBoolean.cpp
  ${BREP_KERNEL_DIR}/src/bool/SphereRecognize.cpp
  ${BREP_KERNEL_DIR}/src/bool/SphereBoxBoolean.cpp
  ${BREP_KERNEL_DIR}/src/bool/SphereSphereBoolean.cpp
  ${BREP_KERNEL_DIR}/src/bool/Classify.cpp
  ${BREP_KERNEL_DIR}/src/bool/Broadphase.cpp
  ${BREP_KERNEL_DIR}/src/spatial/FaceBvh.cpp
)
brep_kernel_include_dirs(brep_core)
target_link_libraries(brep_core PUBLIC brep_eigen brep_boost_uuid brep_spdlog)
set_target_properties(brep_core PROPERTIES OUTPUT_NAME brep_core)

if(MSVC)
  target_compile_options(brep_core PUBLIC /utf-8)
endif()
