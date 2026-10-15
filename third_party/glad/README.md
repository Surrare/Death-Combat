# GLAD 2 — загрузчик OpenGL 4.6 core

Сгенерировано один раз и закоммичено (ARCHITECTURE.md §12.2). Пересоздать:

```bash
python -m pip install glad2
python -m glad --api "gl:core=4.6" --extensions "GL_KHR_debug" --reproducible --out-path third_party/glad c
```

Использование: `#include <glad/gl.h>`, загрузка — `gladLoadGL(glfwGetProcAddress)` (см. `engine/platform/window.cpp`).
