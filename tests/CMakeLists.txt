add_executable(brep_test_math kernel/test_math.cpp)
target_link_libraries(brep_test_math PRIVATE brep GTest::gtest_main)
include(GoogleTest)
gtest_discover_tests(brep_test_math
  WORKING_DIRECTORY $<TARGET_FILE_DIR:brep_test_math>
  DISCOVERY_MODE PRE_TEST
)

add_executable(brep_test_snap kernel/test_snap_query.cpp)
target_link_libraries(brep_test_snap PRIVATE brep GTest::gtest_main)
gtest_discover_tests(brep_test_snap
  WORKING_DIRECTORY $<TARGET_FILE_DIR:brep_test_snap>
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "brep_test_snap."
)

add_executable(brep_test_sphere_surface kernel/test_sphere_surface.cpp)
target_link_libraries(brep_test_sphere_surface PRIVATE brep GTest::gtest_main)
gtest_discover_tests(brep_test_sphere_surface
  WORKING_DIRECTORY $<TARGET_FILE_DIR:brep_test_sphere_surface>
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "brep_test_sphere_surface."
)

add_executable(brep_test_make_sphere kernel/test_make_sphere.cpp)
target_link_libraries(brep_test_make_sphere PRIVATE brep GTest::gtest_main)
gtest_discover_tests(brep_test_make_sphere
  WORKING_DIRECTORY $<TARGET_FILE_DIR:brep_test_make_sphere>
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "brep_test_make_sphere."
)

add_executable(brep_test_tessellate_sphere kernel/test_tessellate_sphere.cpp)
target_link_libraries(brep_test_tessellate_sphere PRIVATE brep GTest::gtest_main)
gtest_discover_tests(brep_test_tessellate_sphere
  WORKING_DIRECTORY $<TARGET_FILE_DIR:brep_test_tessellate_sphere>
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "brep_test_tessellate_sphere."
)

add_executable(brep_test_extract_edges kernel/test_extract_edges.cpp)
target_link_libraries(brep_test_extract_edges PRIVATE brep GTest::gtest_main)
gtest_discover_tests(brep_test_extract_edges
  WORKING_DIRECTORY $<TARGET_FILE_DIR:brep_test_extract_edges>
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "brep_test_extract_edges."
)

add_executable(brep_test_boolean_skeleton kernel/test_boolean_skeleton.cpp)
target_link_libraries(brep_test_boolean_skeleton PRIVATE brep GTest::gtest_main)
gtest_discover_tests(brep_test_boolean_skeleton
  WORKING_DIRECTORY $<TARGET_FILE_DIR:brep_test_boolean_skeleton>
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "brep_test_boolean_skeleton."
)

add_executable(brep_test_boolean_feature kernel/test_boolean_feature.cpp)
target_link_libraries(brep_test_boolean_feature PRIVATE brep GTest::gtest_main)
gtest_discover_tests(brep_test_boolean_feature
  WORKING_DIRECTORY $<TARGET_FILE_DIR:brep_test_boolean_feature>
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "brep_test_boolean_feature."
)

add_executable(brep_test_intersect_plane_plane kernel/test_intersect_plane_plane.cpp)
target_link_libraries(brep_test_intersect_plane_plane PRIVATE brep GTest::gtest_main)
gtest_discover_tests(brep_test_intersect_plane_plane
  WORKING_DIRECTORY $<TARGET_FILE_DIR:brep_test_intersect_plane_plane>
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "brep_test_intersect_plane_plane."
)

add_executable(brep_test_box_boolean kernel/test_box_boolean.cpp)
target_link_libraries(brep_test_box_boolean PRIVATE brep GTest::gtest_main)
gtest_discover_tests(brep_test_box_boolean
  WORKING_DIRECTORY $<TARGET_FILE_DIR:brep_test_box_boolean>
  DISCOVERY_MODE PRE_TEST
  TEST_PREFIX "brep_test_box_boolean."
)
