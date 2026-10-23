// Small self-contained 3D math library (column-major matrices, OpenGL conventions).
#pragma once
#include <cmath>
#include <cstdint>
#include <algorithm>

namespace dc {

constexpr float PI = 3.14159265358979323846f;
inline float radians(float deg) { return deg * (PI / 180.0f); }
inline float degrees(float rad) { return rad * (180.0f / PI); }
inline float clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }
inline float lerp(float a, float b, float t) { return a + (b - a) * t; }
inline float smoothstep(float e0, float e1, float x) {
    float t = clampf((x - e0) / (e1 - e0), 0.0f, 1.0f);
    return t * t * (3.0f - 2.0f * t);
}
inline float sign(float v) { return v < 0 ? -1.0f : (v > 0 ? 1.0f : 0.0f); }
inline float moveTowards(float v, float target, float maxDelta) {
    if (std::fabs(target - v) <= maxDelta) return target;
    return v + sign(target - v) * maxDelta;
}
inline float wrapAngle(float a) {
    while (a > PI) a -= 2 * PI;
    while (a < -PI) a += 2 * PI;
    return a;
}

struct Vec2 {
    float x = 0, y = 0;
    Vec2() = default;
    Vec2(float x_, float y_) : x(x_), y(y_) {}
    Vec2 operator+(Vec2 o) const { return {x + o.x, y + o.y}; }
    Vec2 operator-(Vec2 o) const { return {x - o.x, y - o.y}; }
    Vec2 operator*(float s) const { return {x * s, y * s}; }
    float length() const { return std::sqrt(x * x + y * y); }
};

struct Vec3 {
    float x = 0, y = 0, z = 0;
    Vec3() = default;
    Vec3(float x_, float y_, float z_) : x(x_), y(y_), z(z_) {}
    explicit Vec3(float s) : x(s), y(s), z(s) {}
    Vec3 operator+(const Vec3& o) const { return {x + o.x, y + o.y, z + o.z}; }
    Vec3 operator-(const Vec3& o) const { return {x - o.x, y - o.y, z - o.z}; }
    Vec3 operator*(float s) const { return {x * s, y * s, z * s}; }
    Vec3 operator/(float s) const { return {x / s, y / s, z / s}; }
    Vec3 operator*(const Vec3& o) const { return {x * o.x, y * o.y, z * o.z}; }
    Vec3 operator-() const { return {-x, -y, -z}; }
    Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
    Vec3& operator-=(const Vec3& o) { x -= o.x; y -= o.y; z -= o.z; return *this; }
    Vec3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }
    float length() const { return std::sqrt(x * x + y * y + z * z); }
    float lengthSq() const { return x * x + y * y + z * z; }
};
inline Vec3 operator*(float s, const Vec3& v) { return v * s; }
inline float dot(const Vec3& a, const Vec3& b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
inline Vec3 cross(const Vec3& a, const Vec3& b) {
    return {a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x};
}
inline Vec3 normalize(const Vec3& v) {
    float l = v.length();
    return l > 1e-8f ? v / l : Vec3(0, 0, 0);
}
inline Vec3 lerp(const Vec3& a, const Vec3& b, float t) { return a + (b - a) * t; }
inline float distance(const Vec3& a, const Vec3& b) { return (a - b).length(); }

struct Vec4 {
    float x = 0, y = 0, z = 0, w = 0;
    Vec4() = default;
    Vec4(float x_, float y_, float z_, float w_) : x(x_), y(y_), z(z_), w(w_) {}
    Vec4(const Vec3& v, float w_) : x(v.x), y(v.y), z(v.z), w(w_) {}
    Vec3 xyz() const { return {x, y, z}; }
};

// Column-major 4x4 matrix: m[col][row], matching glUniformMatrix4fv with transpose = GL_FALSE.
struct Mat4 {
    float m[4][4];
    Mat4() {
        for (int c = 0; c < 4; ++c)
            for (int r = 0; r < 4; ++r) m[c][r] = (c == r) ? 1.0f : 0.0f;
    }
    static Mat4 identity() { return Mat4(); }
    const float* data() const { return &m[0][0]; }

    Mat4 operator*(const Mat4& b) const {
        Mat4 r;
        for (int c = 0; c < 4; ++c)
            for (int row = 0; row < 4; ++row) {
                float s = 0;
                for (int k = 0; k < 4; ++k) s += m[k][row] * b.m[c][k];
                r.m[c][row] = s;
            }
        return r;
    }
    Vec4 operator*(const Vec4& v) const {
        return {m[0][0] * v.x + m[1][0] * v.y + m[2][0] * v.z + m[3][0] * v.w,
                m[0][1] * v.x + m[1][1] * v.y + m[2][1] * v.z + m[3][1] * v.w,
                m[0][2] * v.x + m[1][2] * v.y + m[2][2] * v.z + m[3][2] * v.w,
                m[0][3] * v.x + m[1][3] * v.y + m[2][3] * v.z + m[3][3] * v.w};
    }
    Vec3 transformPoint(const Vec3& p) const { return (*this * Vec4(p, 1.0f)).xyz(); }
    Vec3 transformDir(const Vec3& d) const { return (*this * Vec4(d, 0.0f)).xyz(); }

    static Mat4 translate(const Vec3& t) {
        Mat4 r;
        r.m[3][0] = t.x; r.m[3][1] = t.y; r.m[3][2] = t.z;
        return r;
    }
    static Mat4 scale(const Vec3& s) {
        Mat4 r;
        r.m[0][0] = s.x; r.m[1][1] = s.y; r.m[2][2] = s.z;
        return r;
    }
    static Mat4 rotateX(float a) {
        Mat4 r; float c = std::cos(a), s = std::sin(a);
        r.m[1][1] = c; r.m[1][2] = s; r.m[2][1] = -s; r.m[2][2] = c;
        return r;
    }
    static Mat4 rotateY(float a) {
        Mat4 r; float c = std::cos(a), s = std::sin(a);
        r.m[0][0] = c; r.m[0][2] = -s; r.m[2][0] = s; r.m[2][2] = c;
        return r;
    }
    static Mat4 rotateZ(float a) {
        Mat4 r; float c = std::cos(a), s = std::sin(a);
        r.m[0][0] = c; r.m[0][1] = s; r.m[1][0] = -s; r.m[1][1] = c;
        return r;
    }
    static Mat4 perspective(float fovyRad, float aspect, float zn, float zf) {
        Mat4 r;
        float f = 1.0f / std::tan(fovyRad * 0.5f);
        r.m[0][0] = f / aspect; r.m[1][1] = f;
        r.m[2][2] = (zf + zn) / (zn - zf); r.m[2][3] = -1.0f;
        r.m[3][2] = (2.0f * zf * zn) / (zn - zf); r.m[3][3] = 0.0f;
        return r;
    }
    static Mat4 ortho(float l, float r_, float b, float t, float zn, float zf) {
        Mat4 r;
        r.m[0][0] = 2.0f / (r_ - l); r.m[1][1] = 2.0f / (t - b); r.m[2][2] = -2.0f / (zf - zn);
        r.m[3][0] = -(r_ + l) / (r_ - l); r.m[3][1] = -(t + b) / (t - b); r.m[3][2] = -(zf + zn) / (zf - zn);
        return r;
    }
    static Mat4 lookAt(const Vec3& eye, const Vec3& target, const Vec3& up) {
        Vec3 f = normalize(target - eye);
        Vec3 s = normalize(cross(f, up));
        Vec3 u = cross(s, f);
        Mat4 r;
        r.m[0][0] = s.x; r.m[1][0] = s.y; r.m[2][0] = s.z;
        r.m[0][1] = u.x; r.m[1][1] = u.y; r.m[2][1] = u.z;
        r.m[0][2] = -f.x; r.m[1][2] = -f.y; r.m[2][2] = -f.z;
        r.m[3][0] = -dot(s, eye); r.m[3][1] = -dot(u, eye); r.m[3][2] = dot(f, eye);
        return r;
    }
};

struct Color {
    float r = 1, g = 1, b = 1, a = 1;
    Color() = default;
    Color(float r_, float g_, float b_, float a_ = 1.0f) : r(r_), g(g_), b(b_), a(a_) {}
    Color operator*(float s) const { return {r * s, g * s, b * s, a}; }
    Color withAlpha(float a_) const { return {r, g, b, a_}; }
    Vec3 rgb() const { return {r, g, b}; }
};
inline Color mix(const Color& a, const Color& b, float t) {
    return {lerp(a.r, b.r, t), lerp(a.g, b.g, t), lerp(a.b, b.b, t), lerp(a.a, b.a, t)};
}

// Deterministic PRNG (xorshift / splitmix style) so maze and AI are reproducible per seed.
struct Rng {
    uint64_t state;
    explicit Rng(uint64_t seed = 0x9E3779B97F4A7C15ull) : state(seed ? seed : 1) {}
    uint32_t next() {
        uint64_t z = (state += 0x9E3779B97F4A7C15ull);
        z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
        z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
        return (uint32_t)((z ^ (z >> 31)) >> 16);
    }
    int range(int lo, int hi) { return lo + (int)(next() % (uint32_t)(hi - lo + 1)); } // inclusive
    float uniform() { return (next() & 0xFFFFFF) / 16777216.0f; }
    float uniform(float lo, float hi) { return lo + (hi - lo) * uniform(); }
    bool chance(float p) { return uniform() < p; }
};

} // namespace dc
