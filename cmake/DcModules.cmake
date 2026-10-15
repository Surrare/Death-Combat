# Общие правила сборки модулей (docs/ARCHITECTURE.md §12.2).

# Флаги MSVC: строгие предупреждения, UTF-8 исходники, чужие заголовки <...> без предупреждений.
function(dc_set_compile_options target)
    target_compile_options(${target} PRIVATE
        /W4 /WX /permissive- /utf-8 /Zc:__cplusplus /MP
        /external:anglebrackets /external:W0)
    target_compile_definitions(${target} PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN GLFW_INCLUDE_NONE)
endfunction()

# dc_add_module(<цель> <папка> INCLUDE_ROOT <корень> [FOLDER <папка в VS>] [PUBLIC_DEPS ...] [PRIVATE_DEPS ...])
#
# Модуль = статическая библиотека из всех .h/.cpp папки. Новый файл достаточно положить в папку:
# CMake увидит его при следующей сборке (CONFIGURE_DEPENDS). Подключение — от корня: #include "sim/types.h".
function(dc_add_module target dir)
    cmake_parse_arguments(ARG "" "INCLUDE_ROOT;FOLDER" "PUBLIC_DEPS;PRIVATE_DEPS" ${ARGN})
    file(GLOB_RECURSE sources CONFIGURE_DEPENDS "${dir}/*.cpp" "${dir}/*.h")
    add_library(${target} STATIC ${sources})
    target_include_directories(${target} PUBLIC "${ARG_INCLUDE_ROOT}")
    target_link_libraries(${target} PUBLIC ${ARG_PUBLIC_DEPS} PRIVATE ${ARG_PRIVATE_DEPS})
    dc_set_compile_options(${target})
    source_group(TREE "${dir}" FILES ${sources})
    set_target_properties(${target} PROPERTIES FOLDER "${ARG_FOLDER}")
endfunction()
