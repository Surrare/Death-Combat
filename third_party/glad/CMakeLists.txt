# GLAD 2: загрузчик OpenGL 4.6 core profile без расширений (сгенерирован один раз, см. third_party/glad/README.md).
# Core profile значит, что устаревших glBegin/glMatrixMode здесь просто нет — они не скомпилируются.
add_library(glad STATIC src/gl.c)
target_include_directories(glad SYSTEM PUBLIC include)
set_target_properties(glad PROPERTIES FOLDER "third_party")
