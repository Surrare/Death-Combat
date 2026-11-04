#include "game/maze.h"
#include <algorithm>
#include <queue>

namespace dc {

namespace {
const MazeTheme THEMES[MAZE_THEME_COUNT] = {
    {"CATACOMBS", "Torch-lit brick corridors under the city. Tight turns, long shadows.",
     11, 1.0f / 7.0f, 3.6f, true,
     {0.26f, 0.25f, 0.27f}, {0.22f, 0.21f, 0.24f}, {0.42f, 0.36f, 0.33f}, {0.12f, 0.1f, 0.1f}, {0.12f, 0.11f, 0.13f}, {0.35f, 0.33f, 0.36f},
     1, 2, 0.28f, 0, {1.0f, 0.55f, 0.22f}, 2.2f,
     {-0.35f, -1.0f, -0.25f}, {0.30f, 0.28f, 0.34f}, 0.22f, {0.02f, 0.015f, 0.03f}, 0.0032f, Color(0.02f, 0.015f, 0.03f), {0.45f, 0.6f, 1.0f}, 0},
    {"MOONLIT GARDEN", "An overgrown hedge maze under the stars. Wide paths, many ways round, nowhere to hide.",
     13, 1.0f / 4.0f, 3.0f, false,
     {0.17f, 0.22f, 0.16f}, {0.14f, 0.19f, 0.13f}, {0.12f, 0.27f, 0.13f}, {0.1f, 0.08f, 0.06f}, {0, 0, 0}, {0.42f, 0.41f, 0.45f},
     1, 0, 0.2f, 1, {1.0f, 0.72f, 0.4f}, 1.9f,
     {0.45f, -1.0f, 0.5f}, {0.5f, 0.58f, 0.9f}, 0.36f, {0.04f, 0.05f, 0.1f}, 0.0018f, Color(0.03f, 0.04f, 0.09f), {0.55f, 0.7f, 1.0f}, 1},
    {"EMBER FOUNDRY", "Iron tunnels beneath the forge. Furnace vents breathe fire through the grates.",
     10, 1.0f / 3.5f, 3.8f, true,
     {0.2f, 0.19f, 0.19f}, {0.17f, 0.16f, 0.16f}, {0.3f, 0.28f, 0.28f}, {0.35f, 0.25f, 0.06f}, {0.1f, 0.09f, 0.09f}, {0.26f, 0.22f, 0.2f},
     3, 3, 0.34f, 2, {1.0f, 0.45f, 0.15f}, 0.6f,
     {0.2f, -1.0f, 0.3f}, {0.6f, 0.4f, 0.3f}, 0.26f, {0.08f, 0.03f, 0.015f}, 0.0028f, Color(0.05f, 0.02f, 0.012f), {1.0f, 0.55f, 0.3f}, 2},
};
}

const MazeTheme& mazeTheme(int id) { return THEMES[((id % MAZE_THEME_COUNT) + MAZE_THEME_COUNT) % MAZE_THEME_COUNT]; }

bool Maze::hasWall(int cx, int cz, int dir) const {
    if (!inBounds(cx, cz)) return true;
    return (walls8_[cellIndex(cx, cz)] & dir) != 0;
}

void Maze::cellOf(const Vec3& p, int& cx, int& cz) const {
    cx = (int)std::floor(p.x / CELL + 0.5f);
    cz = (int)std::floor(p.z / CELL + 0.5f);
}

bool Maze::isDeadEnd(int cx, int cz) const {
    int n = 0;
    for (int d : {N, E, S, W}) if (!hasWall(cx, cz, d)) ++n;
    return n == 1;
}

std::vector<int> Maze::neighbours(int cx, int cz) const {
    std::vector<int> out;
    if (!hasWall(cx, cz, N) && inBounds(cx, cz - 1)) out.push_back(cellIndex(cx, cz - 1));
    if (!hasWall(cx, cz, S) && inBounds(cx, cz + 1)) out.push_back(cellIndex(cx, cz + 1));
    if (!hasWall(cx, cz, E) && inBounds(cx + 1, cz)) out.push_back(cellIndex(cx + 1, cz));
    if (!hasWall(cx, cz, W) && inBounds(cx - 1, cz)) out.push_back(cellIndex(cx - 1, cz));
    return out;
}

void Maze::generate(int w, int h, uint64_t seed, int theme) {
    w_ = w; h_ = h;
    theme_ = ((theme % MAZE_THEME_COUNT) + MAZE_THEME_COUNT) % MAZE_THEME_COUNT;
    const MazeTheme& T = this->theme();
    walls8_.assign((size_t)(w * h), (uint8_t)(N | E | S | W));
    Rng rng(seed);
    // recursive backtracker (iterative)
    std::vector<uint8_t> visited((size_t)(w * h), 0);
    std::vector<int> stack;
    int start = cellIndex(w / 2, h / 2);
    visited[start] = 1; stack.push_back(start);
    while (!stack.empty()) {
        int cur = stack.back();
        int cx = cur % w, cz = cur / w;
        int opts[4]; int dirs[4]; int n = 0;
        if (cz > 0 && !visited[cellIndex(cx, cz - 1)]) { opts[n] = cellIndex(cx, cz - 1); dirs[n++] = N; }
        if (cz < h - 1 && !visited[cellIndex(cx, cz + 1)]) { opts[n] = cellIndex(cx, cz + 1); dirs[n++] = S; }
        if (cx < w - 1 && !visited[cellIndex(cx + 1, cz)]) { opts[n] = cellIndex(cx + 1, cz); dirs[n++] = E; }
        if (cx > 0 && !visited[cellIndex(cx - 1, cz)]) { opts[n] = cellIndex(cx - 1, cz); dirs[n++] = W; }
        if (n == 0) { stack.pop_back(); continue; }
        int k = rng.range(0, n - 1);
        int next = opts[k]; int d = dirs[k];
        int opp = d == N ? S : d == S ? N : d == E ? W : E;
        walls8_[cur] &= (uint8_t)~d;
        walls8_[next] &= (uint8_t)~opp;
        visited[next] = 1;
        stack.push_back(next);
    }
    // knock out some extra walls so there are loops (less frustrating hunts)
    int extra = (int)((float)(w * h) * T.loops);
    for (int i = 0; i < extra; ++i) {
        int cx = rng.range(1, w - 2), cz = rng.range(1, h - 2);
        int d = rng.chance(0.5f) ? E : S;
        int nx = cx + (d == E ? 1 : 0), nz = cz + (d == S ? 1 : 0);
        walls8_[cellIndex(cx, cz)] &= (uint8_t)~d;
        walls8_[cellIndex(nx, nz)] &= (uint8_t)~(d == E ? W : N);
    }
    // wall boxes: for each cell, east and south walls (+ outer boundary)
    walls_.clear();
    float half = CELL * 0.5f, t = WALL * 0.5f;
    for (int cz = 0; cz < h; ++cz)
        for (int cx = 0; cx < w; ++cx) {
            Vec3 c = cellCenter(cx, cz);
            if (hasWall(cx, cz, E)) walls_.push_back({c.x + half - t, c.z - half - t, c.x + half + t, c.z + half + t});
            if (hasWall(cx, cz, S)) walls_.push_back({c.x - half - t, c.z + half - t, c.x + half + t, c.z + half + t});
            if (cx == 0 && hasWall(cx, cz, W)) walls_.push_back({c.x - half - t, c.z - half - t, c.x - half + t, c.z + half + t});
            if (cz == 0 && hasWall(cx, cz, N)) walls_.push_back({c.x - half - t, c.z - half - t, c.x + half + t, c.z - half + t});
        }
    // torches on some walls, pillars in some cells
    torches_.clear();
    pillarCells_.assign((size_t)(w * h), 0);
    for (int cz = 0; cz < h; ++cz)
        for (int cx = 0; cx < w; ++cx) {
            if (rng.chance(T.lightChance)) {
                Vec3 c = cellCenter(cx, cz);
                int d = rng.range(0, 3);
                int dir = d == 0 ? N : d == 1 ? S : d == 2 ? E : W;
                if (hasWall(cx, cz, dir)) {
                    float off = half - WALL * 0.5f - 0.15f;
                    Vec3 p = c + Vec3(dir == E ? off : dir == W ? -off : 0, T.lightHeight, dir == S ? off : dir == N ? -off : 0);
                    torches_.push_back(p);
                }
            }
            if (rng.chance(0.08f) && !isDeadEnd(cx, cz)) pillarCells_[cellIndex(cx, cz)] = 1;
        }
}

bool Maze::pointInWall(const Vec3& p, float m) const {
    for (const AABB2& b : walls_)
        if (p.x > b.x0 - m && p.x < b.x1 + m && p.z > b.z0 - m && p.z < b.z1 + m) return true;
    // pillars
    for (int cz = 0; cz < h_; ++cz)
        for (int cx = 0; cx < w_; ++cx)
            if (pillarCells_[cellIndex(cx, cz)]) {
                Vec3 c = cellCenter(cx, cz);
                if (std::fabs(p.x - c.x) < 0.35f + m && std::fabs(p.z - c.z) < 0.35f + m) return true;
            }
    return false;
}

Vec3 Maze::collideCircle(const Vec3& pos, float r) const {
    Vec3 p = pos;
    auto resolve = [&](const AABB2& b) {
        float cx = clampf(p.x, b.x0, b.x1), cz = clampf(p.z, b.z0, b.z1);
        float dx = p.x - cx, dz = p.z - cz;
        float d2 = dx * dx + dz * dz;
        if (d2 >= r * r) return;
        if (d2 < 1e-8f) {
            // centre inside the box: push out along the shortest axis
            float lx = p.x - b.x0, rx = b.x1 - p.x, lz = p.z - b.z0, rz = b.z1 - p.z;
            float m = std::min(std::min(lx, rx), std::min(lz, rz));
            if (m == lx) p.x = b.x0 - r; else if (m == rx) p.x = b.x1 + r; else if (m == lz) p.z = b.z0 - r; else p.z = b.z1 + r;
            return;
        }
        float d = std::sqrt(d2);
        p.x = cx + dx / d * r; p.z = cz + dz / d * r;
    };
    for (int iter = 0; iter < 3; ++iter) {
        for (const AABB2& b : walls_) resolve(b);
        for (int cz = 0; cz < h_; ++cz)
            for (int cx = 0; cx < w_; ++cx)
                if (pillarCells_[cellIndex(cx, cz)]) {
                    Vec3 c = cellCenter(cx, cz);
                    resolve({c.x - 0.35f, c.z - 0.35f, c.x + 0.35f, c.z + 0.35f});
                }
    }
    // keep inside the outer boundary
    float half = CELL * 0.5f;
    p.x = clampf(p.x, -half + r + WALL, (w_ - 1) * CELL + half - r - WALL);
    p.z = clampf(p.z, -half + r + WALL, (h_ - 1) * CELL + half - r - WALL);
    return p;
}

bool Maze::lineOfSight(const Vec3& a, const Vec3& b) const {
    Vec3 d = b - a; d.y = 0;
    float len = d.length();
    if (len < 0.01f) return true;
    int steps = (int)(len / 0.25f) + 1;
    for (int i = 1; i < steps; ++i) {
        Vec3 p = a + d * ((float)i / steps);
        if (pointInWall(p)) return false;
    }
    return true;
}

std::vector<int> Maze::pathDistances(int from) const {
    std::vector<int> dist((size_t)(w_ * h_), -1);
    if (from < 0 || from >= w_ * h_) return dist;
    std::queue<int> q; q.push(from); dist[from] = 0;
    while (!q.empty()) {
        int c = q.front(); q.pop();
        for (int n : neighbours(c % w_, c / w_))
            if (dist[n] < 0) { dist[n] = dist[c] + 1; q.push(n); }
    }
    return dist;
}

void Maze::buildMesh(MeshBuilder& floorOut, MeshBuilder& wallsOut, MeshBuilder& ceilingOut, MeshBuilder& propsOut) const {
    const MazeTheme& T = theme();
    const float H = T.height;
    float half = CELL * 0.5f;
    float minX = -half - WALL, minZ = -half - WALL;
    float maxX = (w_ - 1) * CELL + half + WALL, maxZ = (h_ - 1) * CELL + half + WALL;
    floorOut.addPlane(Vec3((minX + maxX) * 0.5f, 0, (minZ + maxZ) * 0.5f), maxX - minX, maxZ - minZ, std::max(w_, h_) * 2, T.floorA, T.floorB);
    // ceiling (normals facing down)
    if (T.ceiling)
        ceilingOut.addQuad(Vec3(minX, H, minZ), Vec3(maxX, H, minZ), Vec3(maxX, H, maxZ), Vec3(minX, H, maxZ), T.ceilingColor);
    Rng rng(99);
    for (const AABB2& b : walls_) {
        float shade = 0.85f + 0.3f * rng.uniform();
        Vec3 c((b.x0 + b.x1) * 0.5f, 0, (b.z0 + b.z1) * 0.5f), sz(b.x1 - b.x0, 0, b.z1 - b.z0);
        wallsOut.addBox(c + Vec3(0, H * 0.5f, 0), Vec3(sz.x, H, sz.z), T.wall * shade);
        // a strip along the base: skirting, or hazard paint in the foundry
        wallsOut.addBox(c + Vec3(0, 0.12f, 0), Vec3(sz.x + 0.08f, 0.24f, sz.z + 0.08f), T.skirting);
        if (T.lightStyle == 1) {
            // hedges: leafy bumps break up the flat walls and the top edge
            int n = 2 + (int)(std::max(sz.x, sz.z) / 1.2f);
            for (int k = 0; k < n; ++k) {
                bool alongX = sz.x > sz.z;
                float u = rng.uniform(-0.5f, 0.5f) * (alongX ? sz.x : sz.z);
                Vec3 p = c + Vec3(alongX ? u : 0, rng.uniform(0.5f, H), alongX ? 0 : u);
                float s = rng.uniform(0.45f, 0.9f);
                propsOut.addBox(p, Vec3(alongX ? s : sz.x + 0.16f, s, alongX ? sz.z + 0.16f : s), T.wall * rng.uniform(0.75f, 1.25f));
            }
        } else if (T.lightStyle == 2) {
            // foundry: copper pipes running along the walls
            bool alongX = sz.x > sz.z;
            Vec3 pipe = alongX ? Vec3(sz.x, 0.14f, sz.z + 0.22f) : Vec3(sz.x + 0.22f, 0.14f, sz.z);
            wallsOut.addBox(c + Vec3(0, H - 0.55f, 0), pipe, Vec3(0.55f, 0.32f, 0.16f));
            wallsOut.addBox(c + Vec3(0, H - 0.85f, 0), pipe * Vec3(1, 0.7f, 1), Vec3(0.35f, 0.33f, 0.32f));
        }
    }
    for (int cz = 0; cz < h_; ++cz)
        for (int cx = 0; cx < w_; ++cx)
            if (pillarCells_[cellIndex(cx, cz)]) {
                Vec3 c = cellCenter(cx, cz);
                propsOut.addBox(c + Vec3(0, H * 0.5f, 0), Vec3(0.7f, H, 0.7f), T.pillar);
                propsOut.addBox(c + Vec3(0, 0.2f, 0), Vec3(1.0f, 0.4f, 1.0f), T.pillar * 0.85f);
                propsOut.addBox(c + Vec3(0, H - 0.2f, 0), Vec3(1.0f, 0.4f, 1.0f), T.pillar * 0.85f);
            }
    for (const Vec3& t : torches_) {
        if (T.lightStyle == 0) {
            propsOut.addBox(t - Vec3(0, 0.25f, 0), Vec3(0.12f, 0.5f, 0.12f), Vec3(0.2f, 0.12f, 0.08f));
            propsOut.addBox(t + Vec3(0, 0.08f, 0), Vec3(0.2f, 0.16f, 0.2f), Vec3(1.0f, 0.6f, 0.2f));
        } else if (T.lightStyle == 1) {
            // lantern on a short iron bracket
            propsOut.addBox(t + Vec3(0, 0.3f, 0), Vec3(0.06f, 0.35f, 0.06f), Vec3(0.12f, 0.12f, 0.13f));
            propsOut.addBox(t, Vec3(0.28f, 0.34f, 0.28f), Vec3(0.12f, 0.12f, 0.13f));
            propsOut.addBox(t, Vec3(0.22f, 0.26f, 0.3f), T.lightColor);
            propsOut.addBox(t, Vec3(0.3f, 0.26f, 0.22f), T.lightColor);
        } else {
            // furnace vent: a glowing grate low on the wall
            propsOut.addBox(t, Vec3(0.75f, 0.5f, 0.75f), Vec3(0.12f, 0.1f, 0.1f));
            for (int k = -1; k <= 1; ++k) propsOut.addBox(t + Vec3(0, k * 0.13f, 0), Vec3(0.8f, 0.06f, 0.8f), T.lightColor);
        }
    }
}

} // namespace dc
