add_executable(brep_test_math kernel/test_math.cpp)
target_link_libraries(brep_test_math PRIVATE brep GTest::gtest_main)
include(GoogleTest)
gtest_discover_tests(brep_test_math)
