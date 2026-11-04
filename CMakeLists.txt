# Death Combat — cross-platform 3D fighting game (C++17, OpenGL 3.3 core).
#
#   Linux:    cmake -B build -DCMAKE_BUILD_TYPE=Release && cmake --build build && ./build/DeathCombat
#   Windows:  cmake -B build && cmake --build build --config Release   (Visual Studio / MinGW / clang)
#
# The only external dependency is GLFW (audio uses the vendored single-header miniaudio).
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

# Bundle the exact font bytes in the executable; no system font or runtime path is required.
file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/generated")
function(dc_embed_font filename symbol)
    set(font_path "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/${filename}")
    set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${font_path}")
    file(READ "${font_path}" font_hex HEX)
    string(REGEX REPLACE "(..)" "0x\\1," DC_FONT_BYTES "${font_hex}")
    string(REPEAT "0x[0-9a-f][0-9a-f]," 16 font_line)
    string(REGEX REPLACE "(${font_line})" "\\1\n" DC_FONT_BYTES "${DC_FONT_BYTES}")
    set(DC_FONT_SYMBOL "${symbol}")
    configure_file(cmake/font_data.h.in "generated/${symbol}.h" @ONLY)
endfunction()
dc_embed_font(BarlowSemiCondensed-Medium.ttf font_body)
dc_embed_font(BarlowSemiCondensed-SemiBold.ttf font_display)
add_library(dc_font STATIC src/core/font.cpp)
target_include_directories(dc_font PUBLIC src PRIVATE "${CMAKE_CURRENT_BINARY_DIR}/generated")
target_include_directories(dc_font SYSTEM PRIVATE third_party)
if(MSVC)
    target_compile_options(dc_font PRIVATE /W4 /utf-8)
endif()

# ---- Game ---------------------------------------------------------------------
file(GLOB_RECURSE DC_SOURCES CONFIGURE_DEPENDS src/*.cpp src/*.h)
list(REMOVE_ITEM DC_SOURCES "${CMAKE_CURRENT_SOURCE_DIR}/src/core/font.cpp")
add_executable(DeathCombat ${DC_SOURCES})
target_include_directories(DeathCombat PRIVATE src third_party)
target_link_libraries(DeathCombat PRIVATE glfw glad dc_font)
add_custom_command(TARGET DeathCombat POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:DeathCombat>/licenses"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/assets/fonts/OFL.txt"
        "$<TARGET_FILE_DIR:DeathCombat>/licenses/Barlow-OFL.txt"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb/LICENSE"
        "$<TARGET_FILE_DIR:DeathCombat>/licenses/stb-LICENSE.txt")
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
    target_link_libraries(DeathCombat PRIVATE opengl32)   # miniaudio uses WASAPI via system DLLs, nothing extra to link
elseif(APPLE)
    target_link_libraries(DeathCombat PRIVATE "-framework OpenGL" "-framework CoreFoundation" "-framework CoreAudio" "-framework AudioToolbox")
else()
    find_package(Threads REQUIRED)
    target_link_libraries(DeathCombat PRIVATE Threads::Threads ${CMAKE_DL_LIBS} m)
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

    # Exercise the rendered rig's geometry on the CPU, without creating a GL context.
    add_executable(DeathCombatRigTest tests/rig_test.cpp src/game/rig.cpp src/game/characters.cpp
        src/gfx/renderer.cpp src/gfx/mesh.cpp src/gfx/shader.cpp)
    target_include_directories(DeathCombatRigTest PRIVATE src)
    target_link_libraries(DeathCombatRigTest PRIVATE glad dc_font ${CMAKE_DL_LIBS})
    if(MSVC)
        target_compile_options(DeathCombatRigTest PRIVATE /W4 /utf-8)
        target_compile_definitions(DeathCombatRigTest PRIVATE NOMINMAX _CRT_SECURE_NO_WARNINGS)
    else()
        target_compile_options(DeathCombatRigTest PRIVATE -Wall -Wextra -Wno-unused-parameter)
    endif()
    add_test(NAME rig_grounding COMMAND DeathCombatRigTest)

    add_executable(DeathCombatFontTest tests/font_test.cpp)
    target_link_libraries(DeathCombatFontTest PRIVATE dc_font)
    if(MSVC)
        target_compile_options(DeathCombatFontTest PRIVATE /W4 /utf-8)
    endif()
    add_test(NAME font_layout COMMAND DeathCombatFontTest)
endif()
