# Death Combat — корневой CMake. Открывается в Visual Studio через «Открыть папку» (CMakePresets.json).
# Архитектура и правила зависимостей: docs/ARCHITECTURE.md §2 и §12.
cmake_minimum_required(VERSION 3.28)
project(DeathCombat VERSION 0.1.0 LANGUAGES C CXX)

if(NOT MSVC)
    message(FATAL_ERROR "Death Combat собирается только MSVC (Visual Studio), см. ADR-0001 в docs/ARCHITECTURE.md")
endif()

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set_property(GLOBAL PROPERTY USE_FOLDERS ON)

include(cmake/DcModules.cmake)

# Зависимости ставит vcpkg из vcpkg.json (ADR-0002)
find_package(glfw3 CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(spdlog CONFIG REQUIRED)
find_package(doctest CONFIG REQUIRED)
find_package(Stb REQUIRED)
find_path(CGLTF_INCLUDE_DIR cgltf.h REQUIRED)
find_path(MINIAUDIO_INCLUDE_DIR miniaudio.h PATH_SUFFIXES miniaudio REQUIRED)

add_subdirectory(third_party/glad)   # загрузчик OpenGL 4.6 core, сгенерирован GLAD 2
add_subdirectory(engine)             # ничего не знает о файтинге
add_subdirectory(game)               # симуляция, ИИ, представление, сцены, DeathCombat.exe

enable_testing()
add_subdirectory(tests)
