# Движок: ничего не знает о файтинге (docs/ARCHITECTURE.md §2). Каждая папка — отдельная статическая библиотека.
# Правило: engine/* никогда не подключает game/*.
set(root "${CMAKE_CURRENT_SOURCE_DIR}")

dc_add_module(dc_core "${root}/core" INCLUDE_ROOT "${root}" FOLDER engine
    PUBLIC_DEPS spdlog::spdlog)
# В Debug игра читает assets прямо из исходников: правки JSON и шейдеров видны без пересборки.
target_compile_definitions(dc_core PRIVATE DC_SOURCE_ASSETS_DIR="${CMAKE_SOURCE_DIR}/assets")

dc_add_module(dc_platform "${root}/platform" INCLUDE_ROOT "${root}" FOLDER engine
    PUBLIC_DEPS dc_core
    PRIVATE_DEPS glfw glad)

dc_add_module(dc_gfx "${root}/gfx" INCLUDE_ROOT "${root}" FOLDER engine
    PUBLIC_DEPS dc_core glad glm::glm imgui::imgui)

dc_add_module(dc_anim "${root}/anim" INCLUDE_ROOT "${root}" FOLDER engine
    PUBLIC_DEPS dc_core glm::glm)

dc_add_module(dc_audio "${root}/audio" INCLUDE_ROOT "${root}" FOLDER engine
    PUBLIC_DEPS dc_core)
target_include_directories(dc_audio SYSTEM PRIVATE "${MINIAUDIO_INCLUDE_DIR}")

dc_add_module(dc_assets "${root}/assets" INCLUDE_ROOT "${root}" FOLDER engine
    PUBLIC_DEPS dc_core nlohmann_json::nlohmann_json)
target_include_directories(dc_assets SYSTEM PRIVATE "${Stb_INCLUDE_DIR}" "${CGLTF_INCLUDE_DIR}")

dc_add_module(dc_input "${root}/input" INCLUDE_ROOT "${root}" FOLDER engine
    PUBLIC_DEPS dc_core glfw)

dc_add_module(dc_ui "${root}/ui" INCLUDE_ROOT "${root}" FOLDER engine
    PUBLIC_DEPS dc_core dc_gfx)
