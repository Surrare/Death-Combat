# CMake generated Testfile for 
# Source directory: /home/dus/Softgays/Death-Combat
# Build directory: /home/dus/Softgays/Death-Combat/build
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test(sim_smoke "/home/dus/Softgays/Death-Combat/build/DeathCombatSimTest")
set_tests_properties(sim_smoke PROPERTIES  _BACKTRACE_TRIPLES "/home/dus/Softgays/Death-Combat/CMakeLists.txt;82;add_test;/home/dus/Softgays/Death-Combat/CMakeLists.txt;0;")
subdirs("third_party/glad")
