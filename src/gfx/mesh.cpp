#include "gfx/mesh.h"

namespace dc {

void MeshBuilder::addTriangle(const Vertex& a, const Vertex& b, const Vertex& c) {
    uint32_t base = (uint32_t)vertices.size();
    vertices.push_back(a); vertices.push_back(b); vertices.push_back(c);
    indices.push_back(base); indices.push_back(base + 1); indices.push_back(base + 2);
}

void MeshBuilder::addQuad(const Vec3& a, const Vec3& b, const Vec3& c, const Vec3& d, const Vec3& color) {
    Vec3 n = normalize(cross(b - a, c - a));
    uint32_t base = (uint32_t)vertices.size();
    vertices.push_back({a, n, color});
    vertices.push_back({b, n, color});
    vertices.push_back({c, n, color});
    vertices.push_back({d, n, color});
    indices.push_back(base); indices.push_back(base + 1); indices.push_back(base + 2);
    indices.push_back(base); indices.push_back(base + 2); indices.push_back(base + 3);
}

void MeshBuilder::addBox(const Vec3& c, const Vec3& s, const Vec3& color) {
    addBoxFaces(c, s, color, true, true, true, true, true, true);
}

void MeshBuilder::addBoxFaces(const Vec3& c, const Vec3& s, const Vec3& color,
                              bool px, bool nx, bool py, bool ny, bool pz, bool nz) {
    Vec3 h = s * 0.5f;
    Vec3 p[8] = {
        {c.x - h.x, c.y - h.y, c.z - h.z}, {c.x + h.x, c.y - h.y, c.z - h.z},
        {c.x + h.x, c.y + h.y, c.z - h.z}, {c.x - h.x, c.y + h.y, c.z - h.z},
        {c.x - h.x, c.y - h.y, c.z + h.z}, {c.x + h.x, c.y - h.y, c.z + h.z},
        {c.x + h.x, c.y + h.y, c.z + h.z}, {c.x - h.x, c.y + h.y, c.z + h.z},
    };
    if (pz) addQuad(p[4], p[5], p[6], p[7], color);
    if (nz) addQuad(p[1], p[0], p[3], p[2], color);
    if (px) addQuad(p[5], p[1], p[2], p[6], color);
    if (nx) addQuad(p[0], p[4], p[7], p[3], color);
    if (py) addQuad(p[7], p[6], p[2], p[3], color);
    if (ny) addQuad(p[0], p[1], p[5], p[4], color);
}

void MeshBuilder::addCylinder(const Vec3& base, float radius, float height, int segments, const Vec3& color) {
    uint32_t start = (uint32_t)vertices.size();
    for (int i = 0; i <= segments; ++i) {
        float a = (float)i / segments * 2 * PI;
        Vec3 n(std::cos(a), 0, std::sin(a));
        vertices.push_back({base + n * radius, n, color});
        vertices.push_back({base + n * radius + Vec3(0, height, 0), n, color});
    }
    for (int i = 0; i < segments; ++i) {
        uint32_t b = start + i * 2;
        indices.push_back(b); indices.push_back(b + 2); indices.push_back(b + 3);
        indices.push_back(b); indices.push_back(b + 3); indices.push_back(b + 1);
    }
    // caps
    for (int cap = 0; cap < 2; ++cap) {
        float y = cap ? height : 0.0f;
        Vec3 n(0, cap ? 1.0f : -1.0f, 0);
        uint32_t c = (uint32_t)vertices.size();
        vertices.push_back({base + Vec3(0, y, 0), n, color});
        for (int i = 0; i <= segments; ++i) {
            float a = (float)i / segments * 2 * PI;
            vertices.push_back({base + Vec3(std::cos(a) * radius, y, std::sin(a) * radius), n, color});
        }
        for (int i = 0; i < segments; ++i) {
            if (cap) { indices.push_back(c); indices.push_back(c + 2 + i); indices.push_back(c + 1 + i); }
            else     { indices.push_back(c); indices.push_back(c + 1 + i); indices.push_back(c + 2 + i); }
        }
    }
}

void MeshBuilder::addSphere(const Vec3& center, float radius, int rings, int segments, const Vec3& color) {
    uint32_t start = (uint32_t)vertices.size();
    for (int r = 0; r <= rings; ++r) {
        float phi = PI * r / rings;
        for (int s = 0; s <= segments; ++s) {
            float theta = 2 * PI * s / segments;
            Vec3 n(std::sin(phi) * std::cos(theta), std::cos(phi), std::sin(phi) * std::sin(theta));
            vertices.push_back({center + n * radius, n, color});
        }
    }
    for (int r = 0; r < rings; ++r)
        for (int s = 0; s < segments; ++s) {
            uint32_t a = start + r * (segments + 1) + s;
            uint32_t b = a + segments + 1;
            indices.push_back(a); indices.push_back(a + 1); indices.push_back(b);
            indices.push_back(a + 1); indices.push_back(b + 1); indices.push_back(b);
        }
}

void MeshBuilder::addCapsule(const Vec3& base, float radius, float height, int segments, const Vec3& color) {
    float cyl = std::max(0.0f, height - 2 * radius);
    addCylinder(base + Vec3(0, radius, 0), radius, cyl, segments, color);
    addSphere(base + Vec3(0, radius, 0), radius, 6, segments, color);
    addSphere(base + Vec3(0, radius + cyl, 0), radius, 6, segments, color);
}

void MeshBuilder::addPlane(const Vec3& c, float sx, float sz, int subdiv, const Vec3& colorA, const Vec3& colorB) {
    for (int i = 0; i < subdiv; ++i)
        for (int j = 0; j < subdiv; ++j) {
            float x0 = c.x - sx / 2 + sx * i / subdiv, x1 = x0 + sx / subdiv;
            float z0 = c.z - sz / 2 + sz * j / subdiv, z1 = z0 + sz / subdiv;
            Vec3 col = ((i + j) & 1) ? colorA : colorB;
            addQuad({x0, c.y, z0}, {x0, c.y, z1}, {x1, c.y, z1}, {x1, c.y, z0}, col);
        }
}

void MeshBuilder::append(const MeshBuilder& o, const Mat4& t) {
    uint32_t base = (uint32_t)vertices.size();
    for (const Vertex& v : o.vertices)
        vertices.push_back({t.transformPoint(v.position), normalize(t.transformDir(v.normal)), v.color});
    for (uint32_t i : o.indices) indices.push_back(base + i);
}

Mesh::~Mesh() {
    if (vao_) glDeleteVertexArrays(1, &vao_);
    if (vbo_) glDeleteBuffers(1, &vbo_);
    if (ebo_) glDeleteBuffers(1, &ebo_);
}

void Mesh::upload(const MeshBuilder& b) {
    if (!vao_) {
        glGenVertexArrays(1, &vao_);
        glGenBuffers(1, &vbo_);
        glGenBuffers(1, &ebo_);
    }
    glBindVertexArray(vao_);
    glBindBuffer(GL_ARRAY_BUFFER, vbo_);
    glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(b.vertices.size() * sizeof(Vertex)), b.vertices.data(), GL_STATIC_DRAW);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, (GLsizeiptr)(b.indices.size() * sizeof(uint32_t)), b.indices.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, position));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, normal));
    glEnableVertexAttribArray(2);
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void*)offsetof(Vertex, color));
    glBindVertexArray(0);
    indexCount_ = (GLsizei)b.indices.size();
}

void Mesh::draw() const {
    if (!valid()) return;
    glBindVertexArray(vao_);
    glDrawElements(GL_TRIANGLES, indexCount_, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

} // namespace dc
