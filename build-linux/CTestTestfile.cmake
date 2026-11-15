# CMake generated Testfile for 
# Source directory: /home/dus/Softgays/Death-Combat
# Build directory: /home/dus/Softgays/Death-Combat/build-linux
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(sim_smoke "/home/dus/Softgays/Death-Combat/build-linux/DeathCombatSimTest")
set_tests_properties(sim_smoke PROPERTIES  _CMAKE_TEST_BUILD_DEPENDS "/home/dus/Softgays/Death-Combat/build-linux/DeathCombatSimTest" _BACKTRACE_TRIPLES "/home/dus/Softgays/Death-Combat/CMakeLists.txt;157;add_test;/home/dus/Softgays/Death-Combat/CMakeLists.txt;0;")
add_test(rig_grounding "/home/dus/Softgays/Death-Combat/build-linux/DeathCombatRigTest")
set_tests_properties(rig_grounding PROPERTIES  _CMAKE_TEST_BUILD_DEPENDS "/home/dus/Softgays/Death-Combat/build-linux/DeathCombatRigTest" _BACKTRACE_TRIPLES "/home/dus/Softgays/Death-Combat/CMakeLists.txt;170;add_test;/home/dus/Softgays/Death-Combat/CMakeLists.txt;0;")
add_test(font_layout "/home/dus/Softgays/Death-Combat/build-linux/DeathCombatFontTest")
set_tests_properties(font_layout PROPERTIES  _CMAKE_TEST_BUILD_DEPENDS "/home/dus/Softgays/Death-Combat/build-linux/DeathCombatFontTest" _BACKTRACE_TRIPLES "/home/dus/Softgays/Death-Combat/CMakeLists.txt;177;add_test;/home/dus/Softgays/Death-Combat/CMakeLists.txt;0;")
add_test(localization "/home/dus/Softgays/Death-Combat/build-linux/DeathCombatLocalizationTest")
set_tests_properties(localization PROPERTIES  _CMAKE_TEST_BUILD_DEPENDS "/home/dus/Softgays/Death-Combat/build-linux/DeathCombatLocalizationTest" _BACKTRACE_TRIPLES "/home/dus/Softgays/Death-Combat/CMakeLists.txt;187;add_test;/home/dus/Softgays/Death-Combat/CMakeLists.txt;0;")
subdirs("third_party/glad")
