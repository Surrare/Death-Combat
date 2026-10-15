# Файтинг поверх движка (docs/ARCHITECTURE.md §2).
# Главное правило: dc_sim зависит только от dc_core — в симуляции нет GL, звука и часов.
set(root "${CMAKE_CURRENT_SOURCE_DIR}")

dc_add_module(dc_sim "${root}/sim" INCLUDE_ROOT "${root}" FOLDER game
    PUBLIC_DEPS dc_core)

dc_add_module(dc_gamedata "${root}/data" INCLUDE_ROOT "${root}" FOLDER game
    PUBLIC_DEPS dc_sim
    PRIVATE_DEPS dc_assets)

dc_add_module(dc_ai "${root}/ai" INCLUDE_ROOT "${root}" FOLDER game
    PUBLIC_DEPS dc_sim)

dc_add_module(dc_controls "${root}/controls" INCLUDE_ROOT "${root}" FOLDER game
    PUBLIC_DEPS dc_sim dc_input)

dc_add_module(dc_presentation "${root}/presentation" INCLUDE_ROOT "${root}" FOLDER game
    PUBLIC_DEPS dc_sim dc_gfx dc_anim dc_audio)

dc_add_module(dc_hud "${root}/hud" INCLUDE_ROOT "${root}" FOLDER game
    PUBLIC_DEPS dc_sim dc_ui)

dc_add_module(dc_scenes "${root}/scenes" INCLUDE_ROOT "${root}" FOLDER game
    PUBLIC_DEPS dc_sim dc_gamedata dc_ai dc_controls dc_presentation dc_hud dc_platform dc_assets)

# Исполняемый файл: точка входа + App (окно, главный цикл, стек сцен).
file(GLOB_RECURSE app_sources CONFIGURE_DEPENDS "${root}/app/*.cpp" "${root}/app/*.h")
add_executable(DeathCombat "${root}/main.cpp" ${app_sources})
target_link_libraries(DeathCombat PRIVATE dc_scenes)
dc_set_compile_options(DeathCombat)
source_group(TREE "${root}" FILES "${root}/main.cpp" ${app_sources})
set_target_properties(DeathCombat PROPERTIES
    FOLDER game
    VS_DEBUGGER_WORKING_DIRECTORY "$<TARGET_FILE_DIR:DeathCombat>")
set_property(DIRECTORY "${CMAKE_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT DeathCombat)

# Release берёт assets рядом с .exe (Debug читает их прямо из исходников, см. core/file_io.cpp).
add_custom_command(TARGET DeathCombat POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory_if_different
            "${CMAKE_SOURCE_DIR}/assets" "$<TARGET_FILE_DIR:DeathCombat>/assets"
    COMMENT "Копирую assets рядом с DeathCombat.exe")
