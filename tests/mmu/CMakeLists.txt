# tests/mmu/CMakeLists.txt (mmu-ip-skeleton, 13.6)

# GoogleTest target for MMU IP tests
# Two targets: mmu_lib_tests (pure lib, no Plugin framework) + mmu_plugin_tests (Plugin integration)

add_executable(mmu_lib_tests
  test_tlb_unit.cpp
  test_multi_level_tlb.cpp
  test_tlb_factory.cpp
  test_mmu_config_schema.cpp
)
target_link_libraries(mmu_lib_tests PRIVATE GTest::gtest_main cf_plugin)
target_include_directories(mmu_lib_tests PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/bundles
  ${CMAKE_SOURCE_DIR}/ip/mmu
)
add_test(NAME mmu_lib_tests COMMAND mmu_lib_tests)
set_tests_properties(mmu_lib_tests PROPERTIES LABELS "mmu")

add_executable(mmu_plugin_tests
  test_mmu_plugin.cpp
)
target_link_libraries(mmu_plugin_tests PRIVATE GTest::gtest_main cf_plugin cf_plugin_bridge)
target_include_directories(mmu_plugin_tests PRIVATE
  ${CMAKE_SOURCE_DIR}/include
  ${CMAKE_SOURCE_DIR}/bundles
  ${CMAKE_SOURCE_DIR}/ip/mmu
)
add_test(NAME mmu_plugin_tests COMMAND mmu_plugin_tests)
set_tests_properties(mmu_plugin_tests PROPERTIES LABELS "mmu")
