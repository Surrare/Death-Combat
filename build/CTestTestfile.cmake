# CMake generated Testfile for 
# Source directory: C:/Death-Combat
# Build directory: C:/Death-Combat/build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(sim_smoke "C:/Death-Combat/build/DeathCombatSimTest.exe")
set_tests_properties(sim_smoke PROPERTIES  _BACKTRACE_TRIPLES "C:/Death-Combat/CMakeLists.txt;127;add_test;C:/Death-Combat/CMakeLists.txt;0;")
add_test(rig_grounding "C:/Death-Combat/build/DeathCombatRigTest.exe")
set_tests_properties(rig_grounding PROPERTIES  _BACKTRACE_TRIPLES "C:/Death-Combat/CMakeLists.txt;140;add_test;C:/Death-Combat/CMakeLists.txt;0;")
add_test(font_layout "C:/Death-Combat/build/DeathCombatFontTest.exe")
set_tests_properties(font_layout PROPERTIES  _BACKTRACE_TRIPLES "C:/Death-Combat/CMakeLists.txt;147;add_test;C:/Death-Combat/CMakeLists.txt;0;")
add_test(localization "C:/Death-Combat/build/DeathCombatLocalizationTest.exe")
set_tests_properties(localization PROPERTIES  _BACKTRACE_TRIPLES "C:/Death-Combat/CMakeLists.txt;157;add_test;C:/Death-Combat/CMakeLists.txt;0;")
subdirs("third_party/glad")
