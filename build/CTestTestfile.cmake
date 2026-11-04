# CMake generated Testfile for 
# Source directory: /home/dus/Softgays/Death-Combat
# Build directory: /home/dus/Softgays/Death-Combat/build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(sim_smoke "/home/dus/Softgays/Death-Combat/build/DeathCombatSimTest")
set_tests_properties(sim_smoke PROPERTIES  _BACKTRACE_TRIPLES "/home/dus/Softgays/Death-Combat/CMakeLists.txt;122;add_test;/home/dus/Softgays/Death-Combat/CMakeLists.txt;0;")
add_test(rig_grounding "/home/dus/Softgays/Death-Combat/build/DeathCombatRigTest")
set_tests_properties(rig_grounding PROPERTIES  _BACKTRACE_TRIPLES "/home/dus/Softgays/Death-Combat/CMakeLists.txt;135;add_test;/home/dus/Softgays/Death-Combat/CMakeLists.txt;0;")
add_test(font_layout "/home/dus/Softgays/Death-Combat/build/DeathCombatFontTest")
set_tests_properties(font_layout PROPERTIES  _BACKTRACE_TRIPLES "/home/dus/Softgays/Death-Combat/CMakeLists.txt;142;add_test;/home/dus/Softgays/Death-Combat/CMakeLists.txt;0;")
add_test(localization "/home/dus/Softgays/Death-Combat/build/DeathCombatLocalizationTest")
set_tests_properties(localization PROPERTIES  _BACKTRACE_TRIPLES "/home/dus/Softgays/Death-Combat/CMakeLists.txt;152;add_test;/home/dus/Softgays/Death-Combat/CMakeLists.txt;0;")
subdirs("third_party/glad")
