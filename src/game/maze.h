// Procedural corridor maze: generation, collision, line of sight, path distances and mesh.
#pragma once
#include <vector>
#include <cstdint>
#include "core/math.h"
#include "gfx/mesh.h"

namespace dc {

struct AABB2 { float x0, z0, x1, z1; };

class Maze {
public:
    static constexpr float CELL = 4.2f;
    static constexpr float WALL = 0.5f;
    static constexpr float HEIGHT = 3.6f;
    enum Dir { N = 1, E = 2, S = 4, W = 8 };   // N = -z, S = +z, E = +x, W = -x

    void generate(int w, int h, uint64_t seed);
    int width() const { return w_; }
    int height() const { return h_; }
    bool hasWall(int cx, int cz, int dir) const;
    Vec3 cellCenter(int cx, int cz) const { return Vec3(cx * CELL, 0, cz * CELL); }
    void cellOf(const Vec3& p, int& cx, int& cz) const;
    bool inBounds(int cx, int cz) const { return cx >= 0 && cz >= 0 && cx < w_ && cz < h_; }
    int cellIndex(int cx, int cz) const { return cz * w_ + cx; }
    bool isDeadEnd(int cx, int cz) const;

    // moves a circle out of walls; returns the corrected position
    Vec3 collideCircle(const Vec3& pos, float radius) const;
    bool pointInWall(const Vec3& p, float margin = 0.0f) const;
    bool lineOfSight(const Vec3& a, const Vec3& b) const;
    std::vector<int> pathDistances(int fromCell) const;      // BFS in cells, -1 unreachable
    std::vector<int> neighbours(int cx, int cz) const;         // reachable neighbour cell indices

    const std::vector<AABB2>& walls() const { return walls_; }
    const std::vector<Vec3>& torches() const { return torches_; }
    void buildMesh(MeshBuilder& floorOut, MeshBuilder& wallsOut, MeshBuilder& ceilingOut, MeshBuilder& propsOut) const;

private:
    int w_ = 0, h_ = 0;
    std::vector<uint8_t> walls8_;     // wall bitmask per cell
    std::vector<AABB2> walls_;
    std::vector<Vec3> torches_;
    std::vector<uint8_t> pillarCells_;
};

} // namespace dc
