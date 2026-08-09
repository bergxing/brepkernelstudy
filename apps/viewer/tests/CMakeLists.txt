include(GoogleTest)

add_executable(viewer_adapter_tests
  test_scene_adapter.cpp
  test_document_service.cpp
)
set_target_properties(viewer_adapter_tests PROPERTIES AUTOMOC OFF AUTOUIC OFF AUTORCC OFF)
target_link_libraries(viewer_adapter_tests PRIVATE
  viewer_adapter
  GTest::gtest_main
)
gtest_discover_tests(viewer_adapter_tests)
