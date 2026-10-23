# Death Combat — cross-platform 3D fighting game (C++17, OpenGL 3.3 core).
#
#   Linux:    cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build && ./build/DeathCombat
#   Windows:  cmake -B build && cmake --build build --config Release   (Visual Studio / MinGW / clang)
#
# The only external dependency is GLFW. It is taken from the system when available,
# otherwise it is downloaded and built automatically with FetchContent.
cmake_minimum_required(VERSION 3.16)
project(DeathCombat VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()

option(DC_FORCE_FETCH_GLFW "Always download GLFW instead of using the system package" OFF)

# ---- GLFW ---------------------------------------------------------------------
set(DC_HAVE_GLFW OFF)
if(NOT DC_FORCE_FETCH_GLFW)
    find_package(glfw3 3.3 CONFIG QUIET)
    if(glfw3_FOUND)
        set(DC_HAVE_GLFW ON)
        message(STATUS "Death Combat: using system GLFW")
    endif()
endif()
if(NOT DC_HAVE_GLFW)
    message(STATUS "Death Combat: downloading GLFW 3.4 with FetchContent")
    include(FetchContent)
    set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(glfw
        URL https://github.com/glfw/glfw/releases/download/3.4/glfw-3.4.zip
        URL_HASH SHA256=b5ec004b2712fd08e8861dc271428f048775200a2df719ccf575143ba749a3e9)
    FetchContent_MakeAvailable(glfw)
endif()

# ---- GLAD (vendored OpenGL loader) ---------------------------------------------
add_subdirectory(third_party/glad)

# ---- Game ---------------------------------------------------------------------
file(GLOB_RECURSE DC_SOURCES CONFIGURE_DEPENDS src/*.cpp src/*.h)
add_executable(DeathCombat ${DC_SOURCES})
target_include_directories(DeathCombat PRIVATE src)
target_link_libraries(DeathCombat PRIVATE glfw glad)
target_compile_definitions(DeathCombat PRIVATE GLFW_INCLUDE_NONE)
if(MSVC)
    target_compile_options(DeathCombat PRIVATE /W4 /utf-8 /MP)
    target_compile_definitions(DeathCombat PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN _CRT_SECURE_NO_WARNINGS)
    # Keep the console hidden in Release builds, but still use main().
    set_target_properties(DeathCombat PROPERTIES
        LINK_FLAGS_RELEASE "/SUBSYSTEM:WINDOWS /ENTRY:mainCRTStartup")
else()
    target_compile_options(DeathCombat PRIVATE -Wall -Wextra -Wno-unused-parameter)
endif()
if(WIN32)
    target_link_libraries(DeathCombat PRIVATE opengl32)
elseif(APPLE)
    target_link_libraries(DeathCombat PRIVATE "-framework OpenGL")
else()
    find_package(Threads REQUIRED)
    target_link_libraries(DeathCombat PRIVATE Threads::Threads ${CMAKE_DL_LIBS})
endif()
set_property(DIRECTORY PROPERTY VS_STARTUP_PROJECT DeathCombat)

# ---- Headless simulation test (no window / OpenGL) ------------------------------
option(DC_BUILD_TESTS "Build the headless simulation test" ON)
if(DC_BUILD_TESTS)
    add_executable(DeathCombatSimTest tests/sim_test.cpp src/game/fight.cpp src/game/fight_ai.cpp src/game/characters.cpp)
    target_include_directories(DeathCombatSimTest PRIVATE src)
    if(MSVC)
        target_compile_options(DeathCombatSimTest PRIVATE /W4 /utf-8)
        target_compile_definitions(DeathCombatSimTest PRIVATE NOMINMAX _CRT_SECURE_NO_WARNINGS)
    else()
        target_compile_options(DeathCombatSimTest PRIVATE -Wall -Wextra -Wno-unused-parameter)
    endif()
    enable_testing()
    add_test(NAME sim_smoke COMMAND DeathCombatSimTest)
endif()
