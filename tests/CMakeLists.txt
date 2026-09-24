include(GoogleTest)

add_executable(brep_test_aspect kernel/TestAspect.cpp)
target_link_libraries(brep_test_aspect PRIVATE brep GTest::gtest_main)
gtest_discover_tests(brep_test_aspect
  WORKING_DIRECTORY $<TARGET_FILE_DIR:brep_test_aspect>
  DISCOVERY_MODE PRE_TEST
)

add_executable(brep_test_math kernel/TestMath.cpp)
target_link_libraries(brep_test_math PRIVATE brep GTest::gtest_main)
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

add_executable(brep_test_bezier_curve kernel/TestBezierCurve.cpp)
target_link_libraries(brep_test_bezier_curve PRIVATE brep GTest::gtest_main)
gtest_discover_tests(brep_test_bezier_curve
  WORKING_DIRECTORY $<TARGET_FILE_DIR:brep_test_bezier_curve>
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "brep_test_bezier_curve."
)

add_executable(brep_test_nurbs_curve kernel/TestNurbsCurve.cpp)
target_link_libraries(brep_test_nurbs_curve PRIVATE brep GTest::gtest_main)
gtest_discover_tests(brep_test_nurbs_curve
  WORKING_DIRECTORY $<TARGET_FILE_DIR:brep_test_nurbs_curve>
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "brep_test_nurbs_curve."
)

add_executable(brep_test_bezier_wire kernel/TestBezierWire.cpp)
target_link_libraries(brep_test_bezier_wire PRIVATE brep GTest::gtest_main)
gtest_discover_tests(brep_test_bezier_wire
  WORKING_DIRECTORY $<TARGET_FILE_DIR:brep_test_bezier_wire>
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "brep_test_bezier_wire."
)

add_executable(brep_test_nurbs_wire kernel/TestNurbsWire.cpp)
target_link_libraries(brep_test_nurbs_wire PRIVATE brep GTest::gtest_main)
gtest_discover_tests(brep_test_nurbs_wire
  WORKING_DIRECTORY $<TARGET_FILE_DIR:brep_test_nurbs_wire>
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "brep_test_nurbs_wire."
)

add_executable(brep_test_bezier_feature kernel/TestBezierFeature.cpp)
target_link_libraries(brep_test_bezier_feature PRIVATE brep GTest::gtest_main)
gtest_discover_tests(brep_test_bezier_feature
  WORKING_DIRECTORY $<TARGET_FILE_DIR:brep_test_bezier_feature>
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "brep_test_bezier_feature."
)

add_executable(brep_test_nurbs_feature kernel/TestNurbsFeature.cpp)
target_link_libraries(brep_test_nurbs_feature PRIVATE brep GTest::gtest_main)
gtest_discover_tests(brep_test_nurbs_feature
  WORKING_DIRECTORY $<TARGET_FILE_DIR:brep_test_nurbs_feature>
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "brep_test_nurbs_feature."
)

add_executable(brep_test_model_remove_body kernel/TestModelRemoveBody.cpp)
target_link_libraries(brep_test_model_remove_body PRIVATE brep GTest::gtest_main)
gtest_discover_tests(brep_test_model_remove_body
  WORKING_DIRECTORY $<TARGET_FILE_DIR:brep_test_model_remove_body>
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "brep_test_model_remove_body."
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

add_executable(brep_test_boolean_pipeline kernel/TestBooleanPipeline.cpp)
target_link_libraries(brep_test_boolean_pipeline PRIVATE brep GTest::gtest_main)
gtest_discover_tests(brep_test_boolean_pipeline
  WORKING_DIRECTORY $<TARGET_FILE_DIR:brep_test_boolean_pipeline>
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "brep_test_boolean_pipeline."
)

add_executable(brep_test_intersect_plane_plane kernel/TestIntersectPlanePlane.cpp)
target_link_libraries(brep_test_intersect_plane_plane PRIVATE brep GTest::gtest_main)
gtest_discover_tests(brep_test_intersect_plane_plane
  WORKING_DIRECTORY $<TARGET_FILE_DIR:brep_test_intersect_plane_plane>
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "brep_test_intersect_plane_plane."
)

add_executable(brep_test_intersection_graph kernel/TestIntersectionGraph.cpp)
target_link_libraries(brep_test_intersection_graph PRIVATE brep GTest::gtest_main)
gtest_discover_tests(brep_test_intersection_graph
  WORKING_DIRECTORY $<TARGET_FILE_DIR:brep_test_intersection_graph>
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "brep_test_intersection_graph."
)

add_executable(brep_test_topology_copy kernel/TestTopologyCopy.cpp)
target_link_libraries(brep_test_topology_copy PRIVATE brep GTest::gtest_main)
gtest_discover_tests(brep_test_topology_copy
  WORKING_DIRECTORY $<TARGET_FILE_DIR:brep_test_topology_copy>
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "brep_test_topology_copy."
)

add_executable(brep_test_apply_transform kernel/TestApplyTransform.cpp)
target_link_libraries(brep_test_apply_transform PRIVATE brep GTest::gtest_main)
gtest_discover_tests(brep_test_apply_transform
  WORKING_DIRECTORY $<TARGET_FILE_DIR:brep_test_apply_transform>
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "brep_test_apply_transform."
)

add_executable(brep_test_property_sheet kernel/TestPropertySheet.cpp)
target_link_libraries(brep_test_property_sheet PRIVATE brep GTest::gtest_main)
gtest_discover_tests(brep_test_property_sheet
  WORKING_DIRECTORY $<TARGET_FILE_DIR:brep_test_property_sheet>
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "brep_test_property_sheet."
)

add_executable(brep_test_untitled_sphere_extrude_boolean
  kernel/TestUntitledSphereExtrudeBoolean.cpp)
target_link_libraries(brep_test_untitled_sphere_extrude_boolean PRIVATE brep
  GTest::gtest_main)
gtest_discover_tests(brep_test_untitled_sphere_extrude_boolean
  WORKING_DIRECTORY $<TARGET_FILE_DIR:brep_test_untitled_sphere_extrude_boolean>
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "brep_test_untitled_sphere_extrude_boolean."
)

add_executable(brep_test_copied_body kernel/TestCopiedBody.cpp)
target_link_libraries(brep_test_copied_body PRIVATE brep GTest::gtest_main)
gtest_discover_tests(brep_test_copied_body
  WORKING_DIRECTORY $<TARGET_FILE_DIR:brep_test_copied_body>
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "brep_test_copied_body."
)

add_executable(brep_test_imprint_planar kernel/TestImprintPlanar.cpp)
target_link_libraries(brep_test_imprint_planar PRIVATE brep GTest::gtest_main)
gtest_discover_tests(brep_test_imprint_planar
  WORKING_DIRECTORY $<TARGET_FILE_DIR:brep_test_imprint_planar>
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "brep_test_imprint_planar."
)

add_executable(brep_test_solid_classifier kernel/TestSolidClassifier.cpp)
target_link_libraries(brep_test_solid_classifier PRIVATE brep GTest::gtest_main)
gtest_discover_tests(brep_test_solid_classifier
  WORKING_DIRECTORY $<TARGET_FILE_DIR:brep_test_solid_classifier>
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "brep_test_solid_classifier."
)

add_executable(brep_test_boolean_chained kernel/TestBooleanChained.cpp)
target_link_libraries(brep_test_boolean_chained PRIVATE brep GTest::gtest_main)
gtest_discover_tests(brep_test_boolean_chained
  WORKING_DIRECTORY $<TARGET_FILE_DIR:brep_test_boolean_chained>
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "brep_test_boolean_chained."
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

add_executable(brep_test_extrude_winding kernel/TestExtrudeWinding.cpp)
target_link_libraries(brep_test_extrude_winding PRIVATE brep GTest::gtest_main)
gtest_discover_tests(brep_test_extrude_winding
  WORKING_DIRECTORY $<TARGET_FILE_DIR:brep_test_extrude_winding>
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "brep_test_extrude_winding."
)

add_executable(brep_test_extrude_pad_boolean kernel/TestExtrudePadBoolean.cpp)
target_link_libraries(brep_test_extrude_pad_boolean PRIVATE brep GTest::gtest_main)
gtest_discover_tests(brep_test_extrude_pad_boolean
  WORKING_DIRECTORY $<TARGET_FILE_DIR:brep_test_extrude_pad_boolean>
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "brep_test_extrude_pad_boolean."
)

add_executable(brep_test_boolean_size_sweep kernel/TestBooleanSizeSweep.cpp)
target_link_libraries(brep_test_boolean_size_sweep PRIVATE brep GTest::gtest_main)
gtest_discover_tests(brep_test_boolean_size_sweep
  WORKING_DIRECTORY $<TARGET_FILE_DIR:brep_test_boolean_size_sweep>
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "brep_test_boolean_size_sweep."
)

add_executable(brep_test_boolean_sphere_sweep kernel/TestBooleanSphereSweep.cpp)
target_link_libraries(brep_test_boolean_sphere_sweep PRIVATE brep GTest::gtest_main)
gtest_discover_tests(brep_test_boolean_sphere_sweep
  WORKING_DIRECTORY $<TARGET_FILE_DIR:brep_test_boolean_sphere_sweep>
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "brep_test_boolean_sphere_sweep."
)

add_executable(brep_test_boolean_sphere_lock kernel/TestBooleanSphereLock.cpp)
target_link_libraries(brep_test_boolean_sphere_lock PRIVATE brep GTest::gtest_main)
gtest_discover_tests(brep_test_boolean_sphere_lock
  WORKING_DIRECTORY $<TARGET_FILE_DIR:brep_test_boolean_sphere_lock>
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "brep_test_boolean_sphere_lock."
)

add_executable(brep_test_pad_sphere_order kernel/TestPadSphereOrder.cpp)
target_link_libraries(brep_test_pad_sphere_order PRIVATE brep GTest::gtest_main)

add_executable(brep_test_untitled_xl_boolean kernel/TestUntitledXlBoolean.cpp)
target_link_libraries(brep_test_untitled_xl_boolean PRIVATE brep GTest::gtest_main)

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

add_executable(brep_test_tessellate_corner_union
  kernel/TestTessellateCornerUnion.cpp)
target_link_libraries(brep_test_tessellate_corner_union PRIVATE brep
                      GTest::gtest_main)
gtest_discover_tests(brep_test_tessellate_corner_union
  WORKING_DIRECTORY $<TARGET_FILE_DIR:brep_test_tessellate_corner_union>
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "brep_test_tessellate_corner_union."
)

add_executable(brep_test_tessellate_closed_sphere_watertight
  kernel/TestTessellateClosedSphereWatertight.cpp)
target_link_libraries(brep_test_tessellate_closed_sphere_watertight PRIVATE brep
                      GTest::gtest_main)
gtest_discover_tests(brep_test_tessellate_closed_sphere_watertight
  WORKING_DIRECTORY $<TARGET_FILE_DIR:brep_test_tessellate_closed_sphere_watertight>
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "brep_test_tessellate_closed_sphere_watertight."
)

add_executable(brep_test_tessellate_octant_boolean
  kernel/TestTessellateOctantBoolean.cpp)
target_link_libraries(brep_test_tessellate_octant_boolean PRIVATE brep
                      GTest::gtest_main)
gtest_discover_tests(brep_test_tessellate_octant_boolean
  WORKING_DIRECTORY $<TARGET_FILE_DIR:brep_test_tessellate_octant_boolean>
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "brep_test_tessellate_octant_boolean."
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
