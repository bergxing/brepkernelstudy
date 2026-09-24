include(${CMAKE_CURRENT_LIST_DIR}/BrepKernelIncludes.cmake)
include(${CMAKE_CURRENT_LIST_DIR}/BrepLibType.cmake)

if(NOT BREP_KERNEL_DIR)
  message(FATAL_ERROR "BREP_KERNEL_DIR is not set")
endif()

add_library(brep_bool ${BREP_LIB_TYPE}
  ${BREP_KERNEL_DIR}/src/bool/EvaluatorStub.cpp
  ${BREP_KERNEL_DIR}/src/bool/CompositeEvaluator.cpp
  ${BREP_KERNEL_DIR}/src/bool/Pipeline.cpp
  ${BREP_KERNEL_DIR}/src/bool/IntersectorRegistry.cpp
  ${BREP_KERNEL_DIR}/src/bool/IntersectionGraph.cpp
  ${BREP_KERNEL_DIR}/src/bool/TopologyCopy.cpp
  ${BREP_KERNEL_DIR}/src/bool/BooleanBuilder.cpp
  ${BREP_KERNEL_DIR}/src/bool/ImprintEngine.cpp
  ${BREP_KERNEL_DIR}/src/bool/SolidClassifier.cpp
  ${BREP_KERNEL_DIR}/src/bool/FaceSelector.cpp
  ${BREP_KERNEL_DIR}/src/bool/IntersectPlanePlane.cpp
  ${BREP_KERNEL_DIR}/src/bool/IntersectPlaneSphere.cpp
  ${BREP_KERNEL_DIR}/src/bool/IntersectSphereSphere.cpp
  ${BREP_KERNEL_DIR}/src/bool/IntersectPlaneCylinder.cpp
  ${BREP_KERNEL_DIR}/src/bool/IntersectSphereCylinder.cpp
  ${BREP_KERNEL_DIR}/src/bool/BoxRecognize.cpp
  ${BREP_KERNEL_DIR}/src/bool/PlanarRecognize.cpp
  ${BREP_KERNEL_DIR}/src/bool/SphereRecognize.cpp
  ${BREP_KERNEL_DIR}/src/bool/Classify.cpp
  ${BREP_KERNEL_DIR}/src/bool/Broadphase.cpp
)
brep_kernel_include_dirs(brep_bool)
target_link_libraries(brep_bool PUBLIC brep_core)
set_target_properties(brep_bool PROPERTIES OUTPUT_NAME brep_bool)

if(MSVC)
  target_compile_options(brep_bool PUBLIC /utf-8)
endif()
