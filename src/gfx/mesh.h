#pragma once
#include <glad/gl.h>
#include <vector>
#include <cstdint>
#include "core/math.h"

namespace dc {

struct Vertex {
    Vec3 position;
    Vec3 normal;
    Vec3 color;
};

// CPU-side mesh builder for procedural geometry.
class MeshBuilder {
public:
    std::vector<Vertex> vertices;
    std::vector<uint32_t> indices;

    void clear() { vertices.clear(); indices.clear(); }
    void addTriangle(const Vertex& a, const Vertex& b, const Vertex& c);
    void addQuad(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d, const Vec3& color); // CCW
    void addBox(const Vec3& center, const Vec3& size, const Vec3& color);
    void addBoxFaces(const Vec3& center, const Vec3& size, const Vec3& color, bool px, bool nx, bool py, bool ny, bool pz, bool nz);
    void addCylinder(const Vec3& base, float radius, float height, int segments, const Vec3& color);
    void addSphere(const Vec3& center, float radius, int rings, int segments, const Vec3& color);
    void addCapsule(const Vec3& base, float radius, float height, int segments, const Vec3& color);
    void addPlane(const Vec3& center, float sizeX, float sizeZ, int subdiv, const Vec3& colorA, const Vec3& colorB);
    void append(const MeshBuilder& other, const Mat4& transform);
};

// GPU mesh (VAO + VBO + EBO).
class Mesh {
public:
    Mesh() = default;
    ~Mesh();
    Mesh(const Mesh&) = delete;
    Mesh& operator=(const Mesh&) = delete;

    void upload(const MeshBuilder& b);
    void draw() const;
    bool valid() const { return vao_ != 0 && indexCount_ > 0; }

private:
    GLuint vao_ = 0, vbo_ = 0, ebo_ = 0;
    GLsizei indexCount_ = 0;
};

} // namespace dc
