add_executable(brep_test_math kernel/TestMath.cpp)
target_link_libraries(brep_test_math PRIVATE brep GTest::gtest_main)
include(GoogleTest)
gtest_discover_tests(brep_test_math
  WORKING_DIRECTORY $<TARGET_FILE_DIR:brep_test_math>
  DISCOVERY_MODE PRE_TEST
)

add_executable(brep_test_cdt kernel/TestCdt.cpp)
target_link_libraries(brep_test_cdt PRIVATE brep GTest::gtest_main)
gtest_discover_tests(brep_test_cdt
  WORKING_DIRECTORY $<TARGET_FILE_DIR:brep_test_cdt>
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "brep_test_cdt."
)

add_executable(brep_test_loop_sample kernel/TestLoopSample.cpp)
target_link_libraries(brep_test_loop_sample PRIVATE brep GTest::gtest_main)
gtest_discover_tests(brep_test_loop_sample
  WORKING_DIRECTORY $<TARGET_FILE_DIR:brep_test_loop_sample>
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "brep_test_loop_sample."
)

add_executable(brep_test_snap kernel/TestSnapQuery.cpp)
target_link_libraries(brep_test_snap PRIVATE brep GTest::gtest_main)
gtest_discover_tests(brep_test_snap
  WORKING_DIRECTORY $<TARGET_FILE_DIR:brep_test_snap>
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "brep_test_snap."
)

add_executable(brep_test_sphere_surface kernel/TestSphereSurface.cpp)
target_link_libraries(brep_test_sphere_surface PRIVATE brep GTest::gtest_main)
gtest_discover_tests(brep_test_sphere_surface
  WORKING_DIRECTORY $<TARGET_FILE_DIR:brep_test_sphere_surface>
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "brep_test_sphere_surface."
)

add_executable(brep_test_make_sphere kernel/TestMakeSphere.cpp)
target_link_libraries(brep_test_make_sphere PRIVATE brep GTest::gtest_main)
gtest_discover_tests(brep_test_make_sphere
  WORKING_DIRECTORY $<TARGET_FILE_DIR:brep_test_make_sphere>
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "brep_test_make_sphere."
)

add_executable(brep_test_tessellate_sphere kernel/TestTessellateSphere.cpp)
target_link_libraries(brep_test_tessellate_sphere PRIVATE brep GTest::gtest_main)
gtest_discover_tests(brep_test_tessellate_sphere
  WORKING_DIRECTORY $<TARGET_FILE_DIR:brep_test_tessellate_sphere>
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "brep_test_tessellate_sphere."
)

add_executable(brep_test_extract_edges kernel/TestExtractEdges.cpp)
target_link_libraries(brep_test_extract_edges PRIVATE brep GTest::gtest_main)
gtest_discover_tests(brep_test_extract_edges
  WORKING_DIRECTORY $<TARGET_FILE_DIR:brep_test_extract_edges>
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "brep_test_extract_edges."
)

add_executable(brep_test_boolean_skeleton kernel/TestBooleanSkeleton.cpp)
target_link_libraries(brep_test_boolean_skeleton PRIVATE brep GTest::gtest_main)
gtest_discover_tests(brep_test_boolean_skeleton
  WORKING_DIRECTORY $<TARGET_FILE_DIR:brep_test_boolean_skeleton>
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "brep_test_boolean_skeleton."
)

add_executable(brep_test_boolean_feature kernel/TestBooleanFeature.cpp)
target_link_libraries(brep_test_boolean_feature PRIVATE brep GTest::gtest_main)
gtest_discover_tests(brep_test_boolean_feature
  WORKING_DIRECTORY $<TARGET_FILE_DIR:brep_test_boolean_feature>
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "brep_test_boolean_feature."
)

add_executable(brep_test_intersect_plane_plane kernel/TestIntersectPlanePlane.cpp)
target_link_libraries(brep_test_intersect_plane_plane PRIVATE brep GTest::gtest_main)
gtest_discover_tests(brep_test_intersect_plane_plane
  WORKING_DIRECTORY $<TARGET_FILE_DIR:brep_test_intersect_plane_plane>
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "brep_test_intersect_plane_plane."
)

add_executable(brep_test_box_boolean kernel/TestBoxBoolean.cpp)
target_link_libraries(brep_test_box_boolean PRIVATE brep GTest::gtest_main)
gtest_discover_tests(brep_test_box_boolean
  WORKING_DIRECTORY $<TARGET_FILE_DIR:brep_test_box_boolean>
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "brep_test_box_boolean."
)

add_executable(brep_test_inner_loop kernel/TestInnerLoop.cpp)
target_link_libraries(brep_test_inner_loop PRIVATE brep GTest::gtest_main)
gtest_discover_tests(brep_test_inner_loop
  WORKING_DIRECTORY $<TARGET_FILE_DIR:brep_test_inner_loop>
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "brep_test_inner_loop."
)

add_executable(brep_test_extrude_holes kernel/TestExtrudeHoles.cpp)
target_link_libraries(brep_test_extrude_holes PRIVATE brep GTest::gtest_main)
gtest_discover_tests(brep_test_extrude_holes
  WORKING_DIRECTORY $<TARGET_FILE_DIR:brep_test_extrude_holes>
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "brep_test_extrude_holes."
)

add_executable(brep_test_tessellate_inner kernel/TestTessellateInner.cpp)
target_link_libraries(brep_test_tessellate_inner PRIVATE brep GTest::gtest_main)
gtest_discover_tests(brep_test_tessellate_inner
  WORKING_DIRECTORY $<TARGET_FILE_DIR:brep_test_tessellate_inner>
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "brep_test_tessellate_inner."
)

add_executable(brep_test_tessellate_trimmed_sphere
  kernel/TestTessellateTrimmedSphere.cpp)
target_link_libraries(brep_test_tessellate_trimmed_sphere PRIVATE brep
                      GTest::gtest_main)
gtest_discover_tests(brep_test_tessellate_trimmed_sphere
  WORKING_DIRECTORY $<TARGET_FILE_DIR:brep_test_tessellate_trimmed_sphere>
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "brep_test_tessellate_trimmed_sphere."
)

add_executable(brep_test_tessellate_untitled_union
  kernel/TestTessellateUntitledUnion.cpp)
target_link_libraries(brep_test_tessellate_untitled_union PRIVATE brep
                      GTest::gtest_main)
gtest_discover_tests(brep_test_tessellate_untitled_union
  WORKING_DIRECTORY $<TARGET_FILE_DIR:brep_test_tessellate_untitled_union>
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "brep_test_tessellate_untitled_union."
)

add_executable(brep_test_planar_boolean kernel/TestPlanarBoolean.cpp)
target_link_libraries(brep_test_planar_boolean PRIVATE brep GTest::gtest_main)
gtest_discover_tests(brep_test_planar_boolean
  WORKING_DIRECTORY $<TARGET_FILE_DIR:brep_test_planar_boolean>
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "brep_test_planar_boolean."
)

add_executable(brep_test_planar_classify kernel/TestPlanarClassify.cpp)
target_link_libraries(brep_test_planar_classify PRIVATE brep GTest::gtest_main)
gtest_discover_tests(brep_test_planar_classify
  WORKING_DIRECTORY $<TARGET_FILE_DIR:brep_test_planar_classify>
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "brep_test_planar_classify."
)

add_executable(brep_test_intersect_plane_sphere kernel/TestIntersectPlaneSphere.cpp)
target_link_libraries(brep_test_intersect_plane_sphere PRIVATE brep GTest::gtest_main)
gtest_discover_tests(brep_test_intersect_plane_sphere
  WORKING_DIRECTORY $<TARGET_FILE_DIR:brep_test_intersect_plane_sphere>
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "brep_test_intersect_plane_sphere."
)

add_executable(brep_test_intersect_sphere_sphere kernel/TestIntersectSphereSphere.cpp)
target_link_libraries(brep_test_intersect_sphere_sphere PRIVATE brep GTest::gtest_main)
gtest_discover_tests(brep_test_intersect_sphere_sphere
  WORKING_DIRECTORY $<TARGET_FILE_DIR:brep_test_intersect_sphere_sphere>
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "brep_test_intersect_sphere_sphere."
)

add_executable(brep_test_sphere_box_boolean kernel/TestSphereBoxBoolean.cpp)
target_link_libraries(brep_test_sphere_box_boolean PRIVATE brep GTest::gtest_main)
gtest_discover_tests(brep_test_sphere_box_boolean
  WORKING_DIRECTORY $<TARGET_FILE_DIR:brep_test_sphere_box_boolean>
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "brep_test_sphere_box_boolean."
)

add_executable(brep_test_sphere_curved_boolean kernel/TestSphereCurvedBoolean.cpp)
target_link_libraries(brep_test_sphere_curved_boolean PRIVATE brep GTest::gtest_main)
gtest_discover_tests(brep_test_sphere_curved_boolean
  WORKING_DIRECTORY $<TARGET_FILE_DIR:brep_test_sphere_curved_boolean>
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "brep_test_sphere_curved_boolean."
)

add_executable(brep_test_cylinder_surface kernel/TestCylinderSurface.cpp)
target_link_libraries(brep_test_cylinder_surface PRIVATE brep GTest::gtest_main)
gtest_discover_tests(brep_test_cylinder_surface
  WORKING_DIRECTORY $<TARGET_FILE_DIR:brep_test_cylinder_surface>
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "brep_test_cylinder_surface."
)

add_executable(brep_test_cylinder_intersect kernel/TestCylinderIntersect.cpp)
target_link_libraries(brep_test_cylinder_intersect PRIVATE brep GTest::gtest_main)
gtest_discover_tests(brep_test_cylinder_intersect
  WORKING_DIRECTORY $<TARGET_FILE_DIR:brep_test_cylinder_intersect>
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "brep_test_cylinder_intersect."
)

add_executable(brep_test_face_bvh kernel/TestFaceBvh.cpp)
target_link_libraries(brep_test_face_bvh PRIVATE brep GTest::gtest_main)
gtest_discover_tests(brep_test_face_bvh
  WORKING_DIRECTORY $<TARGET_FILE_DIR:brep_test_face_bvh>
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "brep_test_face_bvh."
)

add_executable(brep_test_boolean_broadphase kernel/TestBooleanBroadphase.cpp)
target_link_libraries(brep_test_boolean_broadphase PRIVATE brep GTest::gtest_main)
gtest_discover_tests(brep_test_boolean_broadphase
  WORKING_DIRECTORY $<TARGET_FILE_DIR:brep_test_boolean_broadphase>
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "brep_test_boolean_broadphase."
)
