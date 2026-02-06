#include "cube.h"
#include "solver.h"

#include <array>
#include <cstdint>
#include <iostream>
#include <set>
#include <string>
#include <vector>

struct TestCtx {
    int assertions = 0;
    int failures = 0;
};

static void fail(TestCtx& ctx, const char* file, int line, const std::string& msg) {
    ctx.failures++;
    std::cerr << file << ":" << line << ": " << msg << "\n";
}

#define EXPECT_TRUE(ctx, expr)                                                     \
    do {                                                                           \
        (ctx).assertions++;                                                        \
        if (!(expr)) {                                                             \
            fail((ctx), __FILE__, __LINE__, std::string("EXPECT_TRUE failed: ") + #expr); \
        }                                                                          \
    } while (0)

#define EXPECT_EQ(ctx, a, b)                                                       \
    do {                                                                           \
        (ctx).assertions++;                                                        \
        auto _a = (a);                                                             \
        auto _b = (b);                                                             \
        if (!(_a == _b)) {                                                         \
            fail((ctx), __FILE__, __LINE__,                                        \
                 std::string("EXPECT_EQ failed: ") + #a + " != " + #b);            \
        }                                                                          \
    } while (0)

// ============================================================
// Physical reference model (independent of Cube::applyMove)
// ============================================================

struct Vec3i {
    int x = 0;
    int y = 0;
    int z = 0;
};

struct StickerLoc {
    Vec3i pos;   // cubie position, components in {-1,0,1}
    int dir = 0; // Face enum value (FACE_U..FACE_B): outward normal direction
};

static Vec3i dirToVec(int dir) {
    switch (dir) {
    case FACE_U: return {0, 1, 0};
    case FACE_D: return {0, -1, 0};
    case FACE_L: return {-1, 0, 0};
    case FACE_R: return {1, 0, 0};
    case FACE_F: return {0, 0, 1};
    case FACE_B: return {0, 0, -1};
    default: return {0, 0, 0};
    }
}

static int vecToDir(const Vec3i& v) {
    if (v.x == 0 && v.y == 1 && v.z == 0) return FACE_U;
    if (v.x == 0 && v.y == -1 && v.z == 0) return FACE_D;
    if (v.x == -1 && v.y == 0 && v.z == 0) return FACE_L;
    if (v.x == 1 && v.y == 0 && v.z == 0) return FACE_R;
    if (v.x == 0 && v.y == 0 && v.z == 1) return FACE_F;
    if (v.x == 0 && v.y == 0 && v.z == -1) return FACE_B;
    return -1;
}

static Vec3i rotate90(const Vec3i& v, int axis, int sign) {
    // sign: +1 = +90deg, -1 = -90deg (right-hand rule)
    if (sign != 1 && sign != -1) return v;
    switch (axis) {
    case 0: // X axis
        if (sign > 0) return {v.x, -v.z, v.y};
        return {v.x, v.z, -v.y};
    case 1: // Y axis
        if (sign > 0) return {v.z, v.y, -v.x};
        return {-v.z, v.y, v.x};
    case 2: // Z axis
        if (sign > 0) return {-v.y, v.x, v.z};
        return {v.y, -v.x, v.z};
    default:
        return v;
    }
}

static void rotateSticker(StickerLoc& loc, int axis, int turns) {
    if (turns == 0) return;
    int sign = (turns > 0) ? 1 : -1;
    int n = (turns > 0) ? turns : -turns;
    for (int i = 0; i < n; i++) {
        loc.pos = rotate90(loc.pos, axis, sign);
        Vec3i dv = dirToVec(loc.dir);
        dv = rotate90(dv, axis, sign);
        loc.dir = vecToDir(dv);
    }
}

static StickerLoc indexToLoc(int globalIndex) {
    int face = globalIndex / 9;
    int pos = globalIndex % 9;
    int row = pos / 3;
    int col = pos % 3;

    StickerLoc loc;
    loc.dir = face;
    switch (face) {
    case FACE_U:
        loc.pos.y = 1;
        loc.pos.x = col - 1;
        loc.pos.z = row - 1;
        break;
    case FACE_D:
        loc.pos.y = -1;
        loc.pos.x = col - 1;
        loc.pos.z = 1 - row;
        break;
    case FACE_L:
        loc.pos.x = -1;
        loc.pos.y = 1 - row;
        loc.pos.z = col - 1;
        break;
    case FACE_R:
        loc.pos.x = 1;
        loc.pos.y = 1 - row;
        loc.pos.z = 1 - col;
        break;
    case FACE_F:
        loc.pos.z = 1;
        loc.pos.y = 1 - row;
        loc.pos.x = col - 1;
        break;
    case FACE_B:
        loc.pos.z = -1;
        loc.pos.y = 1 - row;
        loc.pos.x = 1 - col;
        break;
    default:
        break;
    }
    return loc;
}

static int locToIndex(const StickerLoc& loc) {
    int pos = Cube::faceletIndexFor(loc.dir, loc.pos.x, loc.pos.y, loc.pos.z);
    if (pos < 0) return -1;
    return loc.dir * 9 + pos;
}

static void moveToAxisLayerTurns(Move m, int& axis, int& layer, int& turns) {
    // face order: U,D,L,R,F,B == 0..5
    static constexpr int kAxis[6] = {1, 1, 0, 0, 2, 2};
    static constexpr int kLayer[6] = {1, -1, -1, 1, 1, -1};
    // Clockwise turns (as seen from outside the face) in right-hand-rule sign.
    static constexpr int kCwTurns[6] = {-1, 1, 1, -1, -1, 1};

    int idx = static_cast<int>(m);
    int face = idx / 3;
    int type = idx % 3; // 0=CW, 1=CCW, 2=double

    axis = kAxis[face];
    layer = kLayer[face];

    int cw = kCwTurns[face];
    if (type == 0) turns = cw;
    else if (type == 1) turns = -cw;
    else turns = 2;
}

static std::array<Color, 54> applyMovePhysical(const std::array<Color, 54>& in, Move m) {
    std::array<Color, 54> out{};
    bool written[54] = {};

    int axis = 0, layer = 0, turns = 0;
    moveToAxisLayerTurns(m, axis, layer, turns);

    for (int i = 0; i < 54; i++) {
        StickerLoc loc = indexToLoc(i);

        int coord = (axis == 0) ? loc.pos.x : (axis == 1) ? loc.pos.y : loc.pos.z;
        if (coord == layer) {
            rotateSticker(loc, axis, turns);
        }

        int j = locToIndex(loc);
        if (j < 0 || j >= 54) {
            // Should never happen if mappings are consistent.
            continue;
        }

        out[j] = in[i];
        written[j] = true;
    }

    // Ensure we produced a full permutation.
    for (int i = 0; i < 54; i++) {
        if (!written[i]) {
            // Leave it as-is; tests will fail later with a mismatch.
        }
    }
    return out;
}

// ============================================================
// Tests
// ============================================================

static void test_faceletIndexFor_roundtrip(TestCtx& ctx) {
    for (int i = 0; i < 54; i++) {
        StickerLoc loc = indexToLoc(i);
        int j = locToIndex(loc);
        EXPECT_EQ(ctx, i, j);
    }

    // A couple of negative checks.
    EXPECT_EQ(ctx, Cube::faceletIndexFor(FACE_U, 0, 0, 0), -1);
    EXPECT_EQ(ctx, Cube::faceletIndexFor(FACE_F, 0, 0, 0), -1);
    EXPECT_EQ(ctx, Cube::faceletIndexFor(FACE_R, 0, 1, 0), -1);
}

static void test_reset_color_scheme(TestCtx& ctx) {
    Cube c;
    c.reset();

    EXPECT_EQ(ctx, c.getFacelet(FACE_U, 4), Color::White);
    EXPECT_EQ(ctx, c.getFacelet(FACE_D, 4), Color::Yellow);
    EXPECT_EQ(ctx, c.getFacelet(FACE_L, 4), Color::Green);
    EXPECT_EQ(ctx, c.getFacelet(FACE_R, 4), Color::Blue);
    EXPECT_EQ(ctx, c.getFacelet(FACE_F, 4), Color::Red);
    EXPECT_EQ(ctx, c.getFacelet(FACE_B, 4), Color::Orange);
}

static void test_move_matches_physical_model(TestCtx& ctx) {
    for (int m = 0; m < 18; m++) {
        Cube c;
        c.reset();

        auto expected = applyMovePhysical(c.getState(), static_cast<Move>(m));
        c.applyMove(static_cast<Move>(m));

        if (!(c.getState() == expected)) {
            std::cerr << "Mismatch for move " << Cube::moveToString(static_cast<Move>(m)) << "\n";
            for (int i = 0; i < 54; i++) {
                if (c.getState()[i] != expected[i]) {
                    std::cerr << "  first diff at global index " << i
                              << " got=" << Cube::colorName(c.getState()[i])
                              << " exp=" << Cube::colorName(expected[i]) << "\n";
                    break;
                }
            }
            EXPECT_TRUE(ctx, false);
        } else {
            EXPECT_TRUE(ctx, true);
        }
    }
}

static void test_inverse_and_identity(TestCtx& ctx) {
    // Inverse correctness
    for (int m = 0; m < 18; m++) {
        Cube c;
        c.reset();
        auto before = c.getState();

        Move mv = static_cast<Move>(m);
        c.applyMove(mv);
        c.applyMove(Cube::inverseMove(mv));

        EXPECT_TRUE(ctx, c.getState() == before);
    }

    // 4x quarter turn identity, 2x half turn identity
    for (int face = 0; face < 6; face++) {
        // CW
        {
            Cube c;
            c.reset();
            auto before = c.getState();
            Move mv = static_cast<Move>(face * 3 + 0);
            for (int i = 0; i < 4; i++) c.applyMove(mv);
            EXPECT_TRUE(ctx, c.getState() == before);
        }
        // CCW
        {
            Cube c;
            c.reset();
            auto before = c.getState();
            Move mv = static_cast<Move>(face * 3 + 1);
            for (int i = 0; i < 4; i++) c.applyMove(mv);
            EXPECT_TRUE(ctx, c.getState() == before);
        }
        // 180
        {
            Cube c;
            c.reset();
            auto before = c.getState();
            Move mv = static_cast<Move>(face * 3 + 2);
            c.applyMove(mv);
            c.applyMove(mv);
            EXPECT_TRUE(ctx, c.getState() == before);
        }
    }
}

static void test_color_count_invariant(TestCtx& ctx) {
    Cube c;
    c.reset();

    std::vector<Move> seq = {
        Move::U, Move::R, Move::F, Move::D, Move::Lp, Move::B2,
        Move::Up, Move::Rp, Move::Fp, Move::Dp, Move::L, Move::B
    };
    for (auto m : seq) c.applyMove(m);

    int counts[6] = {};
    for (Color col : c.getState()) {
        counts[static_cast<int>(col)]++;
    }
    for (int i = 0; i < 6; i++) {
        EXPECT_EQ(ctx, counts[i], 9);
    }
}

static void test_corner_edge_validity_invariants(TestCtx& ctx) {
    Cube c;
    c.reset();

    // Apply a non-trivial sequence.
    std::vector<Move> seq = {
        Move::R, Move::U, Move::Rp, Move::Up,
        Move::F, Move::U, Move::Fp, Move::Up,
        Move::L2, Move::D, Move::B
    };
    for (auto m : seq) c.applyMove(m);

    std::set<int> corners;
    std::set<int> edges;
    int coSum = 0;
    int eoSum = 0;

    for (int i = 0; i < 8; i++) {
        int cp = c.getCornerPermutation(i);
        int co = c.getCornerOrientation(i);
        EXPECT_TRUE(ctx, cp >= 0 && cp < 8);
        EXPECT_TRUE(ctx, co >= 0 && co < 3);
        corners.insert(cp);
        coSum = (coSum + co) % 3;
    }
    EXPECT_EQ(ctx, (int)corners.size(), 8);
    EXPECT_EQ(ctx, coSum, 0);

    for (int i = 0; i < 12; i++) {
        int ep = c.getEdgePermutation(i);
        int eo = c.getEdgeOrientation(i);
        EXPECT_TRUE(ctx, ep >= 0 && ep < 12);
        EXPECT_TRUE(ctx, eo == 0 || eo == 1);
        edges.insert(ep);
        eoSum = (eoSum + eo) % 2;
    }
    EXPECT_EQ(ctx, (int)edges.size(), 12);
    EXPECT_EQ(ctx, eoSum, 0);
}

static void test_coordinate_functions_solved(TestCtx& ctx) {
    Cube c;
    c.reset();

    EXPECT_EQ(ctx, c.cornerOrientationCoord(), 0);
    EXPECT_EQ(ctx, c.edgeOrientationCoord(), 0);
    EXPECT_EQ(ctx, c.udSliceCoord(), 0);
    EXPECT_EQ(ctx, c.cornerPermutationCoord(), 0);
    EXPECT_EQ(ctx, c.phase2EdgePermutationCoord(), 0);
    EXPECT_EQ(ctx, c.udSlicePermutationCoord(), 0);
}

static void test_isSolvable(TestCtx& ctx) {
    {
        Cube c;
        c.reset();
        EXPECT_TRUE(ctx, c.isSolvable());
    }

    {
        Cube c;
        c.reset();
        c.applyMove(Move::R);
        c.applyMove(Move::U);
        c.applyMove(Move::Rp);
        c.applyMove(Move::Up);
        EXPECT_TRUE(ctx, c.isSolvable());
    }

    // Introduce an invalid state by swapping two stickers across different pieces.
    {
        Cube c;
        c.reset();
        auto s = c.getState();
        std::swap(s[FACE_U * 9 + 8], s[FACE_F * 9 + 0]);
        c.setState(s);
        EXPECT_TRUE(ctx, !c.isSolvable());
    }
}

static void test_solver_solves_small_scrambles(TestCtx& ctx) {
    Solver solver;

    const std::vector<std::vector<Move>> scrambles = {
        {Move::U, Move::R, Move::F, Move::Up},
        {Move::L, Move::D, Move::B, Move::R, Move::U2},
        {Move::F, Move::R, Move::U, Move::Rp, Move::Up, Move::Fp},
    };

    for (const auto& scr : scrambles) {
        Cube cube;
        cube.reset();
        for (auto m : scr) cube.applyMove(m);

        Cube beforeSolve = cube;
        auto solution = solver.solve(cube);

        // Solver should not mutate input cube.
        EXPECT_TRUE(ctx, cube.getState() == beforeSolve.getState());

        Cube work = cube;
        for (auto m : solution) work.applyMove(m);
        EXPECT_TRUE(ctx, work.isSolved());
    }
}

static void test_solver_solves_20_move_scramble(TestCtx& ctx) {
    Solver solver;

    // Deterministic 20-move scramble (face-turn metric).
    const std::vector<Move> scramble = {
        Move::R,  Move::U,  Move::Rp, Move::Up,
        Move::F2, Move::L2, Move::D,  Move::B2,
        Move::U2, Move::R2, Move::Fp, Move::L,
        Move::Dp, Move::B,  Move::U,  Move::R,
        Move::Fp, Move::D2, Move::Lp, Move::B2,
    };

    Cube cube;
    cube.reset();
    for (auto m : scramble) cube.applyMove(m);
    EXPECT_TRUE(ctx, cube.isSolvable());

    Cube beforeSolve = cube;
    auto solution = solver.solve(cube);

    // Solver should not mutate input cube.
    EXPECT_TRUE(ctx, cube.getState() == beforeSolve.getState());

    // Kociemba two-phase typically guarantees <= 31 moves in FTM.
    EXPECT_TRUE(ctx, (int)solution.size() <= 31);

    Cube work = cube;
    for (auto m : solution) work.applyMove(m);
    EXPECT_TRUE(ctx, work.isSolved());
}

static void test_solver_solved_is_empty(TestCtx& ctx) {
    Solver solver;
    Cube c;
    c.reset();
    auto sol = solver.solve(c);
    EXPECT_TRUE(ctx, sol.empty());
}

int main() {
    TestCtx ctx;

    test_faceletIndexFor_roundtrip(ctx);
    test_reset_color_scheme(ctx);
    test_move_matches_physical_model(ctx);
    test_inverse_and_identity(ctx);
    test_color_count_invariant(ctx);
    test_corner_edge_validity_invariants(ctx);
    test_coordinate_functions_solved(ctx);
    test_isSolvable(ctx);
    test_solver_solved_is_empty(ctx);
    test_solver_solves_small_scrambles(ctx);
    test_solver_solves_20_move_scramble(ctx);

    std::cerr << "Assertions: " << ctx.assertions << ", Failures: " << ctx.failures << "\n";
    return (ctx.failures == 0) ? 0 : 1;
}
