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

void Cube::rotateFaceCW(int face) {
    auto& s = state_;
    int b = face * 9;
    // Rotate the face itself clockwise
    Color tmp = s[b + 0];
    s[b + 0] = s[b + 6]; s[b + 6] = s[b + 8]; s[b + 8] = s[b + 2]; s[b + 2] = tmp;
    tmp = s[b + 1];
    s[b + 1] = s[b + 3]; s[b + 3] = s[b + 7]; s[b + 7] = s[b + 5]; s[b + 5] = tmp;
}

void Cube::rotateFaceCCW(int face) {
    rotateFaceCW(face);
    rotateFaceCW(face);
    rotateFaceCW(face);
}

void Cube::cycle4(int a, int b, int c, int d) {
    Color tmp = state_[d];
    state_[d] = state_[c];
    state_[c] = state_[b];
    state_[b] = state_[a];
    state_[a] = tmp;
}

void Cube::applyMove(Move m) {
    auto& s = state_;
    switch (m) {
    case Move::U:
        rotateFaceCW(FACE_U);
        cycle4(I(FACE_F,0), I(FACE_L,0), I(FACE_B,0), I(FACE_R,0));
        cycle4(I(FACE_F,1), I(FACE_L,1), I(FACE_B,1), I(FACE_R,1));
        cycle4(I(FACE_F,2), I(FACE_L,2), I(FACE_B,2), I(FACE_R,2));
        break;
    case Move::Up:
        applyMove(Move::U); applyMove(Move::U); applyMove(Move::U);
        break;
    case Move::U2:
        applyMove(Move::U); applyMove(Move::U);
        break;
    case Move::D:
        rotateFaceCW(FACE_D);
        cycle4(I(FACE_F,6), I(FACE_R,6), I(FACE_B,6), I(FACE_L,6));
        cycle4(I(FACE_F,7), I(FACE_R,7), I(FACE_B,7), I(FACE_L,7));
        cycle4(I(FACE_F,8), I(FACE_R,8), I(FACE_B,8), I(FACE_L,8));
        break;
    case Move::Dp:
        applyMove(Move::D); applyMove(Move::D); applyMove(Move::D);
        break;
    case Move::D2:
        applyMove(Move::D); applyMove(Move::D);
        break;
    case Move::L:
        rotateFaceCW(FACE_L);
        cycle4(I(FACE_U,0), I(FACE_F,0), I(FACE_D,0), I(FACE_B,8));
        cycle4(I(FACE_U,3), I(FACE_F,3), I(FACE_D,3), I(FACE_B,5));
        cycle4(I(FACE_U,6), I(FACE_F,6), I(FACE_D,6), I(FACE_B,2));
        break;
    case Move::Lp:
        applyMove(Move::L); applyMove(Move::L); applyMove(Move::L);
        break;
    case Move::L2:
        applyMove(Move::L); applyMove(Move::L);
        break;
    case Move::R:
        rotateFaceCW(FACE_R);
        cycle4(I(FACE_U,2), I(FACE_B,6), I(FACE_D,2), I(FACE_F,2));
        cycle4(I(FACE_U,5), I(FACE_B,3), I(FACE_D,5), I(FACE_F,5));
        cycle4(I(FACE_U,8), I(FACE_B,0), I(FACE_D,8), I(FACE_F,8));
        break;
    case Move::Rp:
        applyMove(Move::R); applyMove(Move::R); applyMove(Move::R);
        break;
    case Move::R2:
        applyMove(Move::R); applyMove(Move::R);
        break;
    case Move::F:
        rotateFaceCW(FACE_F);
        cycle4(I(FACE_U,6), I(FACE_R,0), I(FACE_D,2), I(FACE_L,8));
        cycle4(I(FACE_U,7), I(FACE_R,3), I(FACE_D,1), I(FACE_L,5));
        cycle4(I(FACE_U,8), I(FACE_R,6), I(FACE_D,0), I(FACE_L,2));
        break;
    case Move::Fp:
        applyMove(Move::F); applyMove(Move::F); applyMove(Move::F);
        break;
    case Move::F2:
        applyMove(Move::F); applyMove(Move::F);
        break;
    case Move::B:
        rotateFaceCW(FACE_B);
        cycle4(I(FACE_U,2), I(FACE_L,0), I(FACE_D,6), I(FACE_R,8));
        cycle4(I(FACE_U,1), I(FACE_L,3), I(FACE_D,7), I(FACE_R,5));
        cycle4(I(FACE_U,0), I(FACE_L,6), I(FACE_D,8), I(FACE_R,2));
        break;
    case Move::Bp:
        applyMove(Move::B); applyMove(Move::B); applyMove(Move::B);
        break;
    case Move::B2:
        applyMove(Move::B); applyMove(Move::B);
        break;
    default:
        break;
    }
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
// Kociemba solver coordinate extraction
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

int Cube::cornerOrientationCoord() const {
    int coord = 0;
    for (int i = 0; i < 7; i++) {
        coord = coord * 3 + getCornerOrientation(i);
    }
    return coord; // 0..2186
}

int Cube::edgeOrientationCoord() const {
    int coord = 0;
    for (int i = 0; i < 11; i++) {
        coord = coord * 2 + getEdgeOrientation(i);
    }
    return coord; // 0..2047
}

int Cube::udSliceCoord() const {
    // Which 4 of the 12 edge positions contain UD-slice edges (FR, FL, BL, BR = indices 8-11)
    bool isSlice[12];
    for (int i = 0; i < 12; i++) {
        int ep = getEdgePermutation(i);
        isSlice[i] = (ep >= 8);
    }

    // Encode as a combination number in the range [0..494] (12 choose 4 - 1).
    // Convention: solved cube (slice edges in positions 8..11) => 0.
    auto binom = [](int n, int k) -> int {
        if (k < 0 || k > n) return 0;
        if (k == 0 || k == n) return 1;
        if (k > n - k) k = n - k;
        int res = 1;
        for (int i = 1; i <= k; i++) {
            res = res * (n - k + i) / i;
        }
        return res;
    };

    int coord = 0;
    int k = 4; // slice edges remaining to place while scanning from high to low
    for (int i = 11; i >= 0; i--) {
        if (isSlice[i]) {
            k--;
        } else if (k > 0) {
            coord += binom(i, k);
        }
    }

    return coord; // 0..494
}

int Cube::cornerPermutationCoord() const {
    int perm[8];
    for (int i = 0; i < 8; i++) perm[i] = getCornerPermutation(i);

    int coord = 0;
    for (int i = 7; i > 0; i--) {
        int s = 0;
        for (int j = i + 1; j < 8; j++) {
            // count inversions - but we need Lehmer code
        }
        // Lehmer code
        s = 0;
        for (int j = 0; j < i; j++) {
            if (perm[j] > perm[i]) s++;
        }
        // Wait, let me use the standard factorial number system
        coord = 0; // restart
        break;
    }

    // Factorial number system (Lehmer code)
    coord = 0;
    for (int i = 0; i < 8; i++) {
        int cnt = 0;
        for (int j = i + 1; j < 8; j++) {
            if (perm[j] < perm[i]) cnt++;
        }
        int fact = 1;
        for (int k = 1; k <= 7 - i; k++) fact *= k;
        coord += cnt * fact;
    }
    return coord; // 0..40319
}

int Cube::phase2EdgePermutationCoord() const {
    // Permutation of the 8 non-slice edges (UR, UF, UL, UB, DR, DF, DL, DB) = edges 0..7
    int perm[8];
    for (int i = 0; i < 8; i++) perm[i] = getEdgePermutation(i);

    int coord = 0;
    for (int i = 0; i < 8; i++) {
        int cnt = 0;
        for (int j = i + 1; j < 8; j++) {
            if (perm[j] < perm[i]) cnt++;
        }
        int fact = 1;
        for (int k = 1; k <= 7 - i; k++) fact *= k;
        coord += cnt * fact;
    }
    return coord; // 0..40319
}

int Cube::udSlicePermutationCoord() const {
    // Permutation of the 4 UD-slice edges among themselves
    // Find where slice edges 8,9,10,11 ended up
    int slicePos[4]; // which edge is at positions 8,9,10,11
    for (int i = 0; i < 4; i++) {
        slicePos[i] = getEdgePermutation(i + 8) - 8;
    }

    int coord = 0;
    for (int i = 0; i < 4; i++) {
        int cnt = 0;
        for (int j = i + 1; j < 4; j++) {
            if (slicePos[j] < slicePos[i]) cnt++;
        }
        int fact = 1;
        for (int k = 1; k <= 3 - i; k++) fact *= k;
        coord += cnt * fact;
    }
    return coord; // 0..23
}
