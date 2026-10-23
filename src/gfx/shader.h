#pragma once
#include <glad/gl.h>
#include <string>
#include <unordered_map>
#include "core/math.h"

namespace dc {

class Shader {
public:
    Shader() = default;
    ~Shader();
    Shader(const Shader&) = delete;
    Shader& operator=(const Shader&) = delete;

    bool compile(const char* vertexSrc, const char* fragmentSrc, const char* debugName);
    void use() const { glUseProgram(program_); }
    GLuint id() const { return program_; }

    void set(const char* name, int v);
    void set(const char* name, float v);
    void set(const char* name, const Vec2& v);
    void set(const char* name, const Vec3& v);
    void set(const char* name, const Vec4& v);
    void set(const char* name, const Color& c);
    void set(const char* name, const Mat4& m);
    void setArray(const char* name, const Vec3* v, int n);
    void setArray(const char* name, const Vec4* v, int n);

private:
    GLint location(const char* name);
    GLuint program_ = 0;
    std::unordered_map<std::string, GLint> locations_;
};

} // namespace dc
