#include "gfx/shader.h"
#include <cstdio>
#include <fstream>
#include <sstream>
#include <vector>

namespace dc {

static std::string readShaderDiskFile(const char* filename, std::string& resolvedOut) {
    std::vector<std::string> prefixes = {"shaders/", "../shaders/", "../../shaders/"};
#ifdef DC_INSTALL_SHADER_DIR
    prefixes.emplace_back(DC_INSTALL_SHADER_DIR "/");
#endif
    for (const std::string& prefix : prefixes) {
        std::string path = prefix + filename;
        std::ifstream f(path, std::ios::in | std::ios::binary);
        if (f.is_open()) {
            std::ostringstream ss;
            ss << f.rdbuf();
            std::string s = ss.str();
            if (!s.empty()) {
                resolvedOut = path;
                return s;
            }
        }
    }
    return {};
}

static GLuint compileStage(GLenum type, const char* src, const char* debugName) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetShaderInfoLog(s, sizeof(log), nullptr, log);
        std::fprintf(stderr, "[shader] %s (%s) compile error:\n%s\n", debugName,
                     type == GL_VERTEX_SHADER ? "vertex" : "fragment", log);
        glDeleteShader(s);
        return 0;
    }
    return s;
}

Shader::~Shader() {
    if (program_) glDeleteProgram(program_);
}

bool Shader::compile(const char* vertexSrc, const char* fragmentSrc, const char* debugName) {
    GLuint vs = compileStage(GL_VERTEX_SHADER, vertexSrc, debugName);
    GLuint fs = compileStage(GL_FRAGMENT_SHADER, fragmentSrc, debugName);
    if (!vs || !fs) {
        if (vs) glDeleteShader(vs);
        if (fs) glDeleteShader(fs);
        return false;
    }
    GLuint newProg = glCreateProgram();
    glAttachShader(newProg, vs);
    glAttachShader(newProg, fs);
    glLinkProgram(newProg);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint ok = 0;
    glGetProgramiv(newProg, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetProgramInfoLog(newProg, sizeof(log), nullptr, log);
        std::fprintf(stderr, "[shader] %s link error:\n%s\n", debugName, log);
        glDeleteProgram(newProg);
        return false;
    }
    if (program_) glDeleteProgram(program_);
    program_ = newProg;
    locations_.clear();
    return true;
}

bool Shader::compileFromFiles(const char* vertFile, const char* fragFile,
                              const char* fallbackVS, const char* fallbackFS, const char* debugName) {
    std::string vsPath, fsPath;
    std::string vsDisk = readShaderDiskFile(vertFile, vsPath);
    std::string fsDisk = readShaderDiskFile(fragFile, fsPath);
    if (!vsDisk.empty() && !fsDisk.empty()) {
        if (compile(vsDisk.c_str(), fsDisk.c_str(), debugName)) {
            std::printf("[shader] loaded %s (%s + %s)\n", debugName, vsPath.c_str(), fsPath.c_str());
            return true;
        }
        std::fprintf(stderr, "[shader] %s disk shader failed to compile, falling back to embedded GLSL\n", debugName);
    } else {
        std::fprintf(stderr, "[shader] %s disk files (%s, %s) not found, using embedded fallback\n", debugName, vertFile, fragFile);
    }
    return compile(fallbackVS, fallbackFS, debugName);
}

GLint Shader::location(const char* name) {
    auto it = locations_.find(name);
    if (it != locations_.end()) return it->second;
    GLint loc = glGetUniformLocation(program_, name);
    locations_[name] = loc;
    return loc;
}

void Shader::set(const char* n, int v) { glUniform1i(location(n), v); }
void Shader::set(const char* n, float v) { glUniform1f(location(n), v); }
void Shader::set(const char* n, const Vec2& v) { glUniform2f(location(n), v.x, v.y); }
void Shader::set(const char* n, const Vec3& v) { glUniform3f(location(n), v.x, v.y, v.z); }
void Shader::set(const char* n, const Vec4& v) { glUniform4f(location(n), v.x, v.y, v.z, v.w); }
void Shader::set(const char* n, const Color& c) { glUniform4f(location(n), c.r, c.g, c.b, c.a); }
void Shader::set(const char* n, const Mat4& m) { glUniformMatrix4fv(location(n), 1, GL_FALSE, m.data()); }
void Shader::setArray(const char* n, const Vec3* v, int count) { glUniform3fv(location(n), count, &v[0].x); }
void Shader::setArray(const char* n, const Vec4* v, int count) { glUniform4fv(location(n), count, &v[0].x); }

} // namespace dc
