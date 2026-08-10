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
