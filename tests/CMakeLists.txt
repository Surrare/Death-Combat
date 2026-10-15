# Тесты (docs/ARCHITECTURE.md §11). Запуск: Test Explorer в Visual Studio или ctest --preset debug.
file(GLOB test_sources CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/*.cpp" "${CMAKE_CURRENT_SOURCE_DIR}/*.h")
add_executable(dc_tests ${test_sources})
target_link_libraries(dc_tests PRIVATE dc_sim dc_gamedata dc_ai dc_controls dc_anim doctest::doctest)
target_compile_definitions(dc_tests PRIVATE DC_TEST_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets")
dc_set_compile_options(dc_tests)
set_target_properties(dc_tests PROPERTIES FOLDER tests)
add_test(NAME dc_tests COMMAND dc_tests)
