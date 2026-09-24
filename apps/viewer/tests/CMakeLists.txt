include(GoogleTest)

add_executable(viewer_adapter_tests
  TestSceneAdapter.cpp
  TestDocumentService.cpp
  TestDocumentScope.cpp
  TestBooleanBackend.cpp
  TestModulePlugin.cpp
  TestAppConfigLoader.cpp
)
set_target_properties(viewer_adapter_tests PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
target_link_libraries(viewer_adapter_tests PRIVATE
  viewer_adapter
  viewer_bootstrap
  brep
  GTest::gtest_main
)

if(TARGET viewer_plugin_sample)
  target_compile_definitions(viewer_adapter_tests PRIVATE
    BREP_HAS_VIEWER_PLUGIN_SAMPLE
  )
  add_dependencies(viewer_adapter_tests viewer_plugin_sample)
endif()

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

add_executable(viewer_command_tests
  TestCommandAspects.cpp
)
set_target_properties(viewer_command_tests PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
target_link_libraries(viewer_command_tests PRIVATE
  viewer_bootstrap
  GTest::gtest_main
)
gtest_discover_tests(viewer_command_tests
  WORKING_DIRECTORY $<TARGET_FILE_DIR:viewer_command_tests>
  DISCOVERY_MODE PRE_TEST
)
