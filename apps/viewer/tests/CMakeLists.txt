include(GoogleTest)

add_executable(viewer_adapter_tests
  TestSceneAdapter.cpp
  TestDocumentService.cpp
)
set_target_properties(viewer_adapter_tests PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
target_link_libraries(viewer_adapter_tests PRIVATE
  viewer_adapter
  GTest::gtest_main
)
# SHARED: discover/run next to DLLs in bin/
gtest_discover_tests(viewer_adapter_tests
  WORKING_DIRECTORY $<TARGET_FILE_DIR:viewer_adapter_tests>
  DISCOVERY_MODE PRE_TEST
)

add_executable(viewer_snap_tests
  TestSnapOverlay.cpp
)
set_target_properties(viewer_snap_tests PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
target_link_libraries(viewer_snap_tests PRIVATE
  viewer_runtime
  GTest::gtest_main
)
gtest_discover_tests(viewer_snap_tests
  WORKING_DIRECTORY $<TARGET_FILE_DIR:viewer_snap_tests>
  DISCOVERY_MODE PRE_TEST
)
