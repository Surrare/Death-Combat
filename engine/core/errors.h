#pragma once

#include <stdexcept>

namespace dc::core {

// Стратегия ошибок (docs/ARCHITECTURE.md §12.3, Core Guidelines E.1):
// исключения бросаются только при загрузке и инициализации и ловятся на границе сцены или в main.
// Внутри тика симуляции и кадра рендера исключений нет.
class Error : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

// Окно, контекст OpenGL, видеокарта без 4.6.
class InitError : public Error {
public:
    using Error::Error;
};

// Файла нет, битый .glb / .png / .wav.
class AssetError : public Error {
public:
    using Error::Error;
};

// Ошибка в JSON-данных: нет поля, значение вне диапазона.
class DataError : public Error {
public:
    using Error::Error;
};

// Шейдер не скомпилировался, framebuffer неполный.
class GlError : public Error {
public:
    using Error::Error;
};

}  // namespace dc::core
