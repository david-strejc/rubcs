#include "cube.h"
#include <algorithm>
#include <cassert>
#include <chrono>

// Facelet indices: face * 9 + position
// Position layout per face:
// 0 1 2
// 3 4 5
// 6 7 8

#define I(face, pos) ((face) * 9 + (pos))

Cube::Cube() {
    reset();
}

int Cube::faceletIndexFor(int face, int x, int y, int z) {
    int row = 0, col = 0;
    switch (face) {
    case FACE_U:
        if (y != 1) return -1;
        row = z + 1;
        col = x + 1;
        break;
    case FACE_D:
        if (y != -1) return -1;
        row = 1 - z;
        col = x + 1;
        break;
    case FACE_L:
        if (x != -1) return -1;
        row = 1 - y;
        col = z + 1;
        break;
    case FACE_R:
        if (x != 1) return -1;
        row = 1 - y;
        col = 1 - z;
        break;
    case FACE_F:
        if (z != 1) return -1;
        row = 1 - y;
        col = x + 1;
        break;
    case FACE_B:
        if (z != -1) return -1;
        row = 1 - y;
        col = 1 - x;
        break;
    default:
        return -1;
    }

    if (row < 0 || row > 2 || col < 0 || col > 2) return -1;
    return row * 3 + col;
}

void Cube::reset() {
    // Map face indices (U,D,L,R,F,B) to standard cube colors.
    // This must stay consistent with the solver's corner/edge color definitions.
    static constexpr Color kFaceColor[6] = {
        Color::White,   // U
        Color::Yellow,  // D
        Color::Green,   // L
        Color::Blue,    // R
        Color::Red,     // F
        Color::Orange,  // B
    };

    for (int f = 0; f < 6; f++) {
        for (int i = 0; i < 9; i++) {
            state_[f * 9 + i] = kFaceColor[f];
        }
    }
}

void Cube::setState(const std::array<Color, 54>& s) {
    state_ = s;
}

void Cube::applyMove(Move m) {
    applyMoveRaw(m);
}

namespace {
struct Vec3i {
    int x = 0;
    int y = 0;
    int z = 0;
};

struct Sticker {
    Vec3i pos;
    int dir = FACE_U;
};

static Vec3i dirVec(int dir) {
    static constexpr Vec3i v[6] = {
        {0, 1, 0}, {0, -1, 0}, {-1, 0, 0}, {1, 0, 0}, {0, 0, 1}, {0, 0, -1}
    };
    return v[dir];
}

static int vecDir(const Vec3i& v) {
    if (v.y == 1) return FACE_U;
    if (v.y == -1) return FACE_D;
    if (v.x == -1) return FACE_L;
    if (v.x == 1) return FACE_R;
    if (v.z == 1) return FACE_F;
    return FACE_B;
}

static Vec3i rot90(Vec3i v, int axis, int sign) {
    if (axis == 0) return sign > 0 ? Vec3i{v.x, -v.z, v.y} : Vec3i{v.x, v.z, -v.y};
    if (axis == 1) return sign > 0 ? Vec3i{v.z, v.y, -v.x} : Vec3i{-v.z, v.y, v.x};
    return sign > 0 ? Vec3i{-v.y, v.x, v.z} : Vec3i{v.y, -v.x, v.z};
}

static Sticker stickerAt(int index) {
    int face = index / 9;
    int pos = index % 9;
    int row = pos / 3;
    int col = pos % 3;
    Sticker s;
    s.dir = face;
    if (face == FACE_U) s.pos = {col - 1, 1, row - 1};
    else if (face == FACE_D) s.pos = {col - 1, -1, 1 - row};
    else if (face == FACE_L) s.pos = {-1, 1 - row, col - 1};
    else if (face == FACE_R) s.pos = {1, 1 - row, 1 - col};
    else if (face == FACE_F) s.pos = {col - 1, 1 - row, 1};
    else s.pos = {1 - col, 1 - row, -1};
    return s;
}

static int stickerIndex(const Sticker& s) {
    int pos = Cube::faceletIndexFor(s.dir, s.pos.x, s.pos.y, s.pos.z);
    return s.dir * 9 + pos;
}

static int coord(const Vec3i& v, int axis) {
    return axis == 0 ? v.x : (axis == 1 ? v.y : v.z);
}

static void moveShape(Move m, int& axis, int& layer, int& turns) {
    static constexpr int axes[6] = {1, 1, 0, 0, 2, 2};
    static constexpr int layers[6] = {1, -1, -1, 1, 1, -1};
    static constexpr int cw[6] = {-1, 1, 1, -1, -1, 1};
    int i = static_cast<int>(m);
    int face = i / 3;
    int type = i % 3;
    axis = axes[face];
    layer = layers[face];
    turns = type == 2 ? 2 : (type == 0 ? cw[face] : -cw[face]);
}
}

void Cube::applyMoveRaw(Move m) {
    if (m == Move::COUNT) return;

    int axis = 0, layer = 0, turns = 0;
    moveShape(m, axis, layer, turns);
    int sign = turns < 0 ? -1 : 1;
    int reps = turns < 0 ? -turns : turns;

    std::array<Color, 54> next = state_;
    bool written[54] = {};
    for (int i = 0; i < 54; i++) {
        Sticker s = stickerAt(i);
        if (coord(s.pos, axis) == layer) {
            for (int j = 0; j < reps; j++) {
                s.pos = rot90(s.pos, axis, sign);
                s.dir = vecDir(rot90(dirVec(s.dir), axis, sign));
            }
        }
        int out = stickerIndex(s);
        assert(out >= 0 && out < 54);
        next[out] = state_[i];
        written[out] = true;
    }
    for (bool ok : written) assert(ok);
    state_ = next;
}

void Cube::scramble(int numMoves) {
    auto seed = std::chrono::steady_clock::now().time_since_epoch().count();
    std::mt19937 rng(seed);
    // Only use quarter/half turns of the 6 faces = 18 moves
    std::uniform_int_distribution<int> dist(0, 17);
    for (int i = 0; i < numMoves; i++) {
        applyMove(static_cast<Move>(dist(rng)));
    }

    // Scrambling via legal moves should always remain solvable; keep a guard anyway.
#ifndef NDEBUG
    assert(isSolvable());
#else
    if (!isSolvable()) reset();
#endif
}

bool Cube::isSolved() const {
    for (int f = 0; f < 6; f++) {
        Color center = state_[f * 9 + 4];
        for (int i = 0; i < 9; i++) {
            if (state_[f * 9 + i] != center) return false;
        }
    }
    return true;
}

bool Cube::isSolvable() const {
    // Basic color count check (necessary but not sufficient).
    int counts[6] = {};
    for (Color c : state_) {
        int idx = static_cast<int>(c);
        if (idx < 0 || idx >= 6) return false;
        counts[idx]++;
    }
    for (int i = 0; i < 6; i++) {
        if (counts[i] != 9) return false;
    }

    // Corner/edge permutation + orientation constraints.
    bool seenCorner[8] = {};
    bool seenEdge[12] = {};

    int cornerPerm[8];
    int edgePerm[12];

    int coSum = 0;
    int eoSum = 0;

    for (int i = 0; i < 8; i++) {
        int cp = getCornerPermutation(i);
        int co = getCornerOrientation(i);
        if (cp < 0 || cp >= 8) return false;
        if (co < 0 || co >= 3) return false;
        if (seenCorner[cp]) return false;
        seenCorner[cp] = true;
        cornerPerm[i] = cp;
        coSum = (coSum + co) % 3;
    }
    if (coSum != 0) return false;

    for (int i = 0; i < 12; i++) {
        int ep = getEdgePermutation(i);
        int eo = getEdgeOrientation(i);
        if (ep < 0 || ep >= 12) return false;
        if (!(eo == 0 || eo == 1)) return false;
        if (seenEdge[ep]) return false;
        seenEdge[ep] = true;
        edgePerm[i] = ep;
        eoSum = (eoSum + eo) % 2;
    }
    if (eoSum != 0) return false;

    // Permutation parity: corners and edges must have the same parity.
    auto parity = [](const int* p, int n) -> int {
        int inv = 0;
        for (int i = 0; i < n; i++) {
            for (int j = i + 1; j < n; j++) {
                if (p[i] > p[j]) inv++;
            }
        }
        return inv & 1;
    };
    if (parity(cornerPerm, 8) != parity(edgePerm, 12)) return false;

    return true;
}

Move Cube::inverseMove(Move m) {
    int idx = static_cast<int>(m);
    int face = idx / 3;
    int type = idx % 3; // 0=CW, 1=CCW, 2=half
    if (type == 0) return static_cast<Move>(face * 3 + 1);
    if (type == 1) return static_cast<Move>(face * 3);
    return m; // half turn is self-inverse
}

std::string Cube::moveToString(Move m) {
    static const char* names[] = {
        "U", "U'", "U2", "D", "D'", "D2",
        "L", "L'", "L2", "R", "R'", "R2",
        "F", "F'", "F2", "B", "B'", "B2"
    };
    return names[static_cast<int>(m)];
}

const char* Cube::colorName(Color c) {
    static const char* names[] = {"White", "Yellow", "Red", "Orange", "Green", "Blue"};
    return names[static_cast<int>(c)];
}

// ============================================================
// Cubie extraction for validation
// ============================================================

// Corner cubies defined by their facelets
// 8 corners: URF, UFL, ULB, UBR, DFR, DLF, DBL, DRB
// Each corner has 3 facelets listed in order: U/D face, then clockwise
static const int cornerFacelets[8][3] = {
    {I(FACE_U,8), I(FACE_R,0), I(FACE_F,2)},  // URF
    {I(FACE_U,6), I(FACE_F,0), I(FACE_L,2)},  // UFL
    {I(FACE_U,0), I(FACE_L,0), I(FACE_B,2)},  // ULB
    {I(FACE_U,2), I(FACE_B,0), I(FACE_R,2)},  // UBR
    {I(FACE_D,2), I(FACE_F,8), I(FACE_R,6)},  // DFR
    {I(FACE_D,0), I(FACE_L,8), I(FACE_F,6)},  // DLF
    {I(FACE_D,6), I(FACE_B,8), I(FACE_L,6)},  // DBL
    {I(FACE_D,8), I(FACE_R,8), I(FACE_B,6)},  // DRB
};

// The colors that each corner should have (matching the solved state)
static const Color cornerColors[8][3] = {
    {Color::White,  Color::Blue,   Color::Red},    // URF
    {Color::White,  Color::Red,    Color::Green},   // UFL
    {Color::White,  Color::Green,  Color::Orange},  // ULB
    {Color::White,  Color::Orange, Color::Blue},    // UBR
    {Color::Yellow, Color::Red,    Color::Blue},    // DFR
    {Color::Yellow, Color::Green,  Color::Red},     // DLF
    {Color::Yellow, Color::Orange, Color::Green},   // DBL
    {Color::Yellow, Color::Blue,   Color::Orange},  // DRB
};

// 12 edges: UR, UF, UL, UB, DR, DF, DL, DB, FR, FL, BL, BR
static const int edgeFacelets[12][2] = {
    {I(FACE_U,5), I(FACE_R,1)},  // UR
    {I(FACE_U,7), I(FACE_F,1)},  // UF
    {I(FACE_U,3), I(FACE_L,1)},  // UL
    {I(FACE_U,1), I(FACE_B,1)},  // UB
    {I(FACE_D,5), I(FACE_R,7)},  // DR
    {I(FACE_D,1), I(FACE_F,7)},  // DF
    {I(FACE_D,3), I(FACE_L,7)},  // DL
    {I(FACE_D,7), I(FACE_B,7)},  // DB
    {I(FACE_F,5), I(FACE_R,3)},  // FR
    {I(FACE_F,3), I(FACE_L,5)},  // FL
    {I(FACE_B,5), I(FACE_L,3)},  // BL
    {I(FACE_B,3), I(FACE_R,5)},  // BR
};

static const Color edgeColors[12][2] = {
    {Color::White,  Color::Blue},    // UR
    {Color::White,  Color::Red},     // UF
    {Color::White,  Color::Green},   // UL
    {Color::White,  Color::Orange},  // UB
    {Color::Yellow, Color::Blue},    // DR
    {Color::Yellow, Color::Red},     // DF
    {Color::Yellow, Color::Green},   // DL
    {Color::Yellow, Color::Orange},  // DB
    {Color::Red,    Color::Blue},    // FR
    {Color::Red,    Color::Green},   // FL
    {Color::Orange, Color::Green},   // BL
    {Color::Orange, Color::Blue},    // BR
};

int Cube::getCornerPermutation(int pos) const {
    Color c0 = state_[cornerFacelets[pos][0]];
    Color c1 = state_[cornerFacelets[pos][1]];
    Color c2 = state_[cornerFacelets[pos][2]];
    for (int c = 0; c < 8; c++) {
        if ((c0 == cornerColors[c][0] || c0 == cornerColors[c][1] || c0 == cornerColors[c][2]) &&
            (c1 == cornerColors[c][0] || c1 == cornerColors[c][1] || c1 == cornerColors[c][2]) &&
            (c2 == cornerColors[c][0] || c2 == cornerColors[c][1] || c2 == cornerColors[c][2])) {
            // Make sure all three are different and match
            if (c0 != c1 && c1 != c2 && c0 != c2)
                return c;
        }
    }
    return -1; // should not happen on valid cube
}

int Cube::getCornerOrientation(int pos) const {
    Color c0 = state_[cornerFacelets[pos][0]];
    // Orientation: 0 if U/D color is on U/D face, 1 if CW, 2 if CCW
    if (c0 == Color::White || c0 == Color::Yellow) return 0;
    Color c1 = state_[cornerFacelets[pos][1]];
    if (c1 == Color::White || c1 == Color::Yellow) return 1;
    return 2;
}

int Cube::getEdgePermutation(int pos) const {
    Color c0 = state_[edgeFacelets[pos][0]];
    Color c1 = state_[edgeFacelets[pos][1]];
    for (int e = 0; e < 12; e++) {
        if ((c0 == edgeColors[e][0] && c1 == edgeColors[e][1]) ||
            (c0 == edgeColors[e][1] && c1 == edgeColors[e][0])) {
            return e;
        }
    }
    return -1;
}

int Cube::getEdgeOrientation(int pos) const {
    Color c0 = state_[edgeFacelets[pos][0]];
    int ep = getEdgePermutation(pos);
    if (ep < 0) return 0;
    return (c0 == edgeColors[ep][0]) ? 0 : 1;
}
