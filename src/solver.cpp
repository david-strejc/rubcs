#include "solver.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <mutex>
#include <utility>
#include <vector>

namespace {

constexpr int kNumMoves = 18;

// Kociemba phase-2 moves: U, U', U2, D, D', D2, L2, R2, F2, B2.
constexpr int kNumPhase2Moves = 10;
constexpr int kPhase2Moves[kNumPhase2Moves] = {
    0, 1, 2,  // U, U', U2
    3, 4, 5,  // D, D', D2
    8, 11,    // L2, R2
    14, 17,   // F2, B2
};

constexpr int kCO = 2187;   // 3^7
constexpr int kEO = 2048;   // 2^11
constexpr int kSLICE = 495; // C(12,4)
constexpr int kCP = 40320;  // 8!
constexpr int kEP = 40320;  // 8!
constexpr int kSP = 24;     // 4!

// Small binomial table for combination ranking/unranking (n<=12, k<=4).
static const std::array<std::array<int, 5>, 13> kBinom = []() {
    std::array<std::array<int, 5>, 13> b{};
    for (int n = 0; n <= 12; n++) {
        for (int k = 0; k <= 4; k++) {
            if (k == 0) b[n][k] = 1;
            else if (k > n) b[n][k] = 0;
            else if (k == n) b[n][k] = 1;
            else b[n][k] = b[n - 1][k - 1] + b[n - 1][k];
        }
    }
    return b;
}();

static constexpr int kFact8[9] = {1, 1, 2, 6, 24, 120, 720, 5040, 40320};
static constexpr int kFact4[5] = {1, 1, 2, 6, 24};

struct CubieCube {
    std::array<uint8_t, 8> cp{}; // which corner cubie is at each corner position
    std::array<uint8_t, 8> co{}; // corner orientation (0..2)
    std::array<uint8_t, 12> ep{}; // which edge cubie is at each edge position
    std::array<uint8_t, 12> eo{}; // edge orientation (0..1)
};

static CubieCube toCubie(const Cube& cube) {
    CubieCube cc{};
    for (int i = 0; i < 8; i++) {
        cc.cp[i] = static_cast<uint8_t>(cube.getCornerPermutation(i));
        cc.co[i] = static_cast<uint8_t>(cube.getCornerOrientation(i));
    }
    for (int i = 0; i < 12; i++) {
        cc.ep[i] = static_cast<uint8_t>(cube.getEdgePermutation(i));
        cc.eo[i] = static_cast<uint8_t>(cube.getEdgeOrientation(i));
    }
    return cc;
}

static int cornerOriCoord(const std::array<uint8_t, 8>& co) {
    int coord = 0;
    for (int i = 0; i < 7; i++) coord = coord * 3 + (co[i] % 3);
    return coord;
}

static std::array<uint8_t, 8> cornerOriFromCoord(int coord) {
    std::array<uint8_t, 8> co{};
    int sum = 0;
    for (int i = 6; i >= 0; i--) {
        co[i] = static_cast<uint8_t>(coord % 3);
        sum += co[i];
        coord /= 3;
    }
    co[7] = static_cast<uint8_t>((3 - (sum % 3)) % 3);
    return co;
}

static int edgeOriCoord(const std::array<uint8_t, 12>& eo) {
    int coord = 0;
    for (int i = 0; i < 11; i++) coord = coord * 2 + (eo[i] & 1);
    return coord;
}

static std::array<uint8_t, 12> edgeOriFromCoord(int coord) {
    std::array<uint8_t, 12> eo{};
    int sum = 0;
    for (int i = 10; i >= 0; i--) {
        eo[i] = static_cast<uint8_t>(coord & 1);
        sum ^= eo[i];
        coord >>= 1;
    }
    eo[11] = static_cast<uint8_t>(sum & 1); // even parity
    return eo;
}

static int sliceCoord(const std::array<uint8_t, 12>& ep) {
    int coord = 0;
    int k = 4;
    for (int i = 11; i >= 0; i--) {
        bool isSlice = ep[i] >= 8;
        if (isSlice) {
            k--;
        } else if (k > 0) {
            coord += kBinom[i][k];
        }
    }
    return coord;
}

static std::array<uint8_t, 12> sliceFromCoord(int coord) {
    // Build an edge permutation where "slice edges" (8..11) occupy the positions
    // indicated by `coord` (in the same ranking convention as sliceCoord()).
    std::array<uint8_t, 12> ep{};
    bool isSlicePos[12] = {};

    int k = 4;
    for (int i = 11; i >= 0; i--) {
        if (k == 0) {
            isSlicePos[i] = false;
            continue;
        }

        int c = kBinom[i][k];
        if (coord >= c) {
            coord -= c;
            isSlicePos[i] = false;
        } else {
            isSlicePos[i] = true;
            k--;
        }
    }

    int nextNS = 0;
    int nextSlice = 8;
    for (int i = 0; i < 12; i++) {
        if (isSlicePos[i]) ep[i] = static_cast<uint8_t>(nextSlice++);
        else ep[i] = static_cast<uint8_t>(nextNS++);
    }
    return ep;
}

static int perm8Coord(const std::array<uint8_t, 8>& p) {
    int coord = 0;
    for (int i = 0; i < 8; i++) {
        int cnt = 0;
        for (int j = i + 1; j < 8; j++) {
            if (p[j] < p[i]) cnt++;
        }
        coord += cnt * kFact8[7 - i];
    }
    return coord;
}

static std::array<uint8_t, 8> perm8FromCoord(int coord) {
    std::array<uint8_t, 8> p{};
    std::array<uint8_t, 8> elems = {0, 1, 2, 3, 4, 5, 6, 7};

    int remaining = 8;
    for (int i = 0; i < 8; i++) {
        int fact = kFact8[remaining - 1];
        int idx = coord / fact;
        coord %= fact;
        p[i] = elems[idx];
        for (int j = idx; j < remaining - 1; j++) elems[j] = elems[j + 1];
        remaining--;
    }
    return p;
}

static int perm4Coord(const std::array<uint8_t, 4>& p) {
    int coord = 0;
    for (int i = 0; i < 4; i++) {
        int cnt = 0;
        for (int j = i + 1; j < 4; j++) {
            if (p[j] < p[i]) cnt++;
        }
        coord += cnt * kFact4[3 - i];
    }
    return coord;
}

static std::array<uint8_t, 4> perm4FromCoord(int coord) {
    std::array<uint8_t, 4> p{};
    std::array<uint8_t, 4> elems = {0, 1, 2, 3};

    int remaining = 4;
    for (int i = 0; i < 4; i++) {
        int fact = kFact4[remaining - 1];
        int idx = coord / fact;
        coord %= fact;
        p[i] = elems[idx];
        for (int j = idx; j < remaining - 1; j++) elems[j] = elems[j + 1];
        remaining--;
    }
    return p;
}

struct Tables {
    // Move effects in cubie representation: for each new position, which old position moved there,
    // and the orientation delta for that piece.
    std::array<std::array<uint8_t, 8>, kNumMoves> cPos{};
    std::array<std::array<uint8_t, 8>, kNumMoves> cOri{};
    std::array<std::array<uint8_t, 12>, kNumMoves> ePos{};
    std::array<std::array<uint8_t, 12>, kNumMoves> eOri{};
    std::array<int, kNumMoves> invMove{}; // move index -> inverse move index

    // Phase 1 move tables.
    std::vector<std::array<uint16_t, kNumMoves>> coMove;    // [2187][18]
    std::vector<std::array<uint16_t, kNumMoves>> eoMove;    // [2048][18]
    std::vector<std::array<uint16_t, kNumMoves>> sliceMove; // [495][18]

    // Phase 2 move tables (for the 10 phase-2 moves).
    std::vector<std::array<uint16_t, kNumPhase2Moves>> cpMove; // [40320][10]
    std::vector<std::array<uint16_t, kNumPhase2Moves>> epMove; // [40320][10]
    std::vector<std::array<uint8_t, kNumPhase2Moves>> spMove;  // [24][10]

    // Phase 1 pruning tables.
    std::vector<uint8_t> pruneCoSlice; // [2187*495]
    std::vector<uint8_t> pruneEoSlice; // [2048*495]

    // Phase 2 pruning tables.
    std::vector<uint8_t> pruneCpSp; // [40320*24]
    std::vector<uint8_t> pruneEpSp; // [40320*24]

    void init();

    void applyMove(CubieCube& cc, int move) const {
        CubieCube out = cc;

        for (int i = 0; i < 8; i++) {
            int oldPos = cPos[move][i];
            out.cp[i] = cc.cp[oldPos];
            out.co[i] = static_cast<uint8_t>((cc.co[oldPos] + cOri[move][i]) % 3);
        }
        for (int i = 0; i < 12; i++) {
            int oldPos = ePos[move][i];
            out.ep[i] = cc.ep[oldPos];
            out.eo[i] = static_cast<uint8_t>(cc.eo[oldPos] ^ eOri[move][i]);
        }
        cc = out;
    }
};

static Tables g_tables;
static std::once_flag g_tablesOnce;

static bool moveAllowedPrune(int move, int lastMove) {
    if (lastMove < 0) return true;
    int face = move / 3;
    int lastFace = lastMove / 3;
    if (face == lastFace) return false;
    if (face / 2 == lastFace / 2 && face < lastFace) return false;
    return true;
}

void Tables::init() {
    // Build move effects from the authoritative facelet move implementation.
    Cube base;
    base.reset();
    CubieCube solved = toCubie(base);
#ifndef NDEBUG
    for (int i = 0; i < 8; i++) {
        // On a solved cube, each position contains its own piece with orientation 0.
        // This keeps piece IDs stable and makes move effect derivation straightforward.
        assert(solved.cp[i] == i);
    }
    for (int i = 0; i < 12; i++) assert(solved.ep[i] == i);
#endif

    for (int m = 0; m < kNumMoves; m++) {
        invMove[m] = static_cast<int>(Cube::inverseMove(static_cast<Move>(m)));

        Cube tmp = base;
        tmp.applyMove(static_cast<Move>(m));
        CubieCube mv = toCubie(tmp);

        for (int i = 0; i < 8; i++) {
            cPos[m][i] = mv.cp[i];
            cOri[m][i] = mv.co[i];
        }
        for (int i = 0; i < 12; i++) {
            ePos[m][i] = mv.ep[i];
            eOri[m][i] = mv.eo[i];
        }
    }

    // ------------------------------------------------------------
    // Phase 1 move tables
    // ------------------------------------------------------------
    coMove.assign(kCO, {});
    eoMove.assign(kEO, {});
    sliceMove.assign(kSLICE, {});

    // CO move table
    for (int co = 0; co < kCO; co++) {
        CubieCube cc = solved;
        cc.co = cornerOriFromCoord(co);
        for (int m = 0; m < kNumMoves; m++) {
            CubieCube t = cc;
            applyMove(t, m);
            coMove[co][m] = static_cast<uint16_t>(cornerOriCoord(t.co));
        }
    }

    // EO move table
    for (int eo = 0; eo < kEO; eo++) {
        CubieCube cc = solved;
        cc.eo = edgeOriFromCoord(eo);
        for (int m = 0; m < kNumMoves; m++) {
            CubieCube t = cc;
            applyMove(t, m);
            eoMove[eo][m] = static_cast<uint16_t>(edgeOriCoord(t.eo));
        }
    }

    // Slice move table
    for (int sl = 0; sl < kSLICE; sl++) {
        CubieCube cc = solved;
        cc.ep = sliceFromCoord(sl);
        for (int m = 0; m < kNumMoves; m++) {
            CubieCube t = cc;
            applyMove(t, m);
            sliceMove[sl][m] = static_cast<uint16_t>(sliceCoord(t.ep));
        }
    }

    // ------------------------------------------------------------
    // Phase 2 move tables (only 10 moves that preserve the phase-2 subgroup)
    // ------------------------------------------------------------
    cpMove.assign(kCP, {});
    epMove.assign(kEP, {});
    spMove.assign(kSP, {});

    // CP move table
    for (int cp = 0; cp < kCP; cp++) {
        CubieCube cc = solved;
        cc.cp = perm8FromCoord(cp);
        for (int mi = 0; mi < kNumPhase2Moves; mi++) {
            int move = kPhase2Moves[mi];
            CubieCube t = cc;
            applyMove(t, move);
            cpMove[cp][mi] = static_cast<uint16_t>(perm8Coord(t.cp));
        }
    }

    // EP move table (permutation of the 8 non-slice edges in positions 0..7).
    for (int ep = 0; ep < kEP; ep++) {
        CubieCube cc = solved;
        auto perm = perm8FromCoord(ep);
        for (int i = 0; i < 8; i++) cc.ep[i] = perm[i];
        for (int i = 8; i < 12; i++) cc.ep[i] = static_cast<uint8_t>(i); // slice edges stay put in phase 2

        for (int mi = 0; mi < kNumPhase2Moves; mi++) {
            int move = kPhase2Moves[mi];
            CubieCube t = cc;
            applyMove(t, move);
            std::array<uint8_t, 8> p{};
            for (int i = 0; i < 8; i++) p[i] = t.ep[i];
            epMove[ep][mi] = static_cast<uint16_t>(perm8Coord(p));
        }
    }

    // Slice permutation move table (permutation of slice edges among positions 8..11).
    for (int sp = 0; sp < kSP; sp++) {
        CubieCube cc = solved;
        auto p4 = perm4FromCoord(sp);
        for (int i = 0; i < 8; i++) cc.ep[i] = static_cast<uint8_t>(i);
        for (int i = 0; i < 4; i++) cc.ep[8 + i] = static_cast<uint8_t>(8 + p4[i]);

        for (int mi = 0; mi < kNumPhase2Moves; mi++) {
            int move = kPhase2Moves[mi];
            CubieCube t = cc;
            applyMove(t, move);
            std::array<uint8_t, 4> q{};
            for (int i = 0; i < 4; i++) q[i] = static_cast<uint8_t>(t.ep[8 + i] - 8);
            spMove[sp][mi] = static_cast<uint8_t>(perm4Coord(q));
        }
    }

    // ------------------------------------------------------------
    // Pruning tables (BFS from solved)
    // ------------------------------------------------------------
    auto buildPrunePhase1 = [&](int sizeA, int sizeB,
                                const auto& moveA, const auto& moveB) -> std::vector<uint8_t> {
        std::vector<uint8_t> prune((size_t)sizeA * (size_t)sizeB, 0xFF);
        std::vector<int> q;
        q.reserve(prune.size());

        prune[0] = 0;
        q.push_back(0);

        for (size_t head = 0; head < q.size(); head++) {
            int idx = q[head];
            int a = idx / sizeB;
            int b = idx % sizeB;
            uint8_t d = prune[idx];
            for (int m = 0; m < kNumMoves; m++) {
                int na = moveA[a][m];
                int nb = moveB[b][m];
                int nidx = na * sizeB + nb;
                if (prune[nidx] != 0xFF) continue;
                prune[nidx] = static_cast<uint8_t>(d + 1);
                q.push_back(nidx);
            }
        }
        return prune;
    };

    pruneCoSlice = buildPrunePhase1(kCO, kSLICE, coMove, sliceMove);
    pruneEoSlice = buildPrunePhase1(kEO, kSLICE, eoMove, sliceMove);

    auto buildPrunePhase2 = [&](const auto& moveA, const auto& moveB) -> std::vector<uint8_t> {
        constexpr int sizeA = kCP; // 40320
        constexpr int sizeB = kSP; // 24
        std::vector<uint8_t> prune((size_t)sizeA * (size_t)sizeB, 0xFF);
        std::vector<int> q;
        q.reserve(prune.size());

        prune[0] = 0;
        q.push_back(0);
        for (size_t head = 0; head < q.size(); head++) {
            int idx = q[head];
            int a = idx / sizeB;
            int b = idx % sizeB;
            uint8_t d = prune[idx];
            for (int mi = 0; mi < kNumPhase2Moves; mi++) {
                int na = moveA[a][mi];
                int nb = moveB[b][mi];
                int nidx = na * sizeB + nb;
                if (prune[nidx] != 0xFF) continue;
                prune[nidx] = static_cast<uint8_t>(d + 1);
                q.push_back(nidx);
            }
        }
        return prune;
    };

    pruneCpSp = buildPrunePhase2(cpMove, spMove);
    pruneEpSp = buildPrunePhase2(epMove, spMove);
}

static const Tables& tables() {
    std::call_once(g_tablesOnce, []() { g_tables.init(); });
    return g_tables;
}

static int edgePermCoord8_fromEp(const std::array<uint8_t, 12>& ep) {
    std::array<uint8_t, 8> p{};
    for (int i = 0; i < 8; i++) p[i] = ep[i];
    return perm8Coord(p);
}

static int slicePermCoord_fromEp(const std::array<uint8_t, 12>& ep) {
    std::array<uint8_t, 4> p{};
    for (int i = 0; i < 4; i++) p[i] = static_cast<uint8_t>(ep[8 + i] - 8);
    return perm4Coord(p);
}

static bool searchPhase2(const Tables& t, int cp, int ep, int sp, int depth, int lastMove,
                         std::vector<int>& path, std::atomic_bool* cancel, SolverProgress* progress) {
    if (cancel && cancel->load(std::memory_order_relaxed)) return false;
    if (progress) progress->nodes.fetch_add(1, std::memory_order_relaxed);
    int idx = cp * kSP + sp;
    int h1 = t.pruneCpSp[idx];
    int h2 = t.pruneEpSp[ep * kSP + sp];
    int h = std::max(h1, h2);
    if (h > depth) return false;

    if (depth == 0) return (cp == 0 && ep == 0 && sp == 0);

    for (int mi = 0; mi < kNumPhase2Moves; mi++) {
        int move = kPhase2Moves[mi];
        if (!moveAllowedPrune(move, lastMove)) continue;

        int ncp = t.cpMove[cp][mi];
        int nep = t.epMove[ep][mi];
        int nsp = t.spMove[sp][mi];

        path.push_back(move);
        if (searchPhase2(t, ncp, nep, nsp, depth - 1, move, path, cancel, progress)) return true;
        path.pop_back();
    }
    return false;
}

static bool searchPhase1(const Tables& t, CubieCube& cc, int depth, int lastMove, std::vector<int>& path1,
                         std::vector<int>& outPhase2, int maxTotalDepth,
                         std::atomic_bool* cancel, SolverProgress* progress) {
    if (cancel && cancel->load(std::memory_order_relaxed)) return false;
    if (progress) progress->nodes.fetch_add(1, std::memory_order_relaxed);

    int co = cornerOriCoord(cc.co);
    int eo = edgeOriCoord(cc.eo);
    int sl = sliceCoord(cc.ep);
    int h1 = t.pruneCoSlice[co * kSLICE + sl];
    int h2 = t.pruneEoSlice[eo * kSLICE + sl];
    int h = std::max(h1, h2);
    if (h > depth) return false;

    if (depth == 0) {
        if (!(co == 0 && eo == 0 && sl == 0)) return false;

        int cp = perm8Coord(cc.cp);
        int ep = edgePermCoord8_fromEp(cc.ep);
        int sp = slicePermCoord_fromEp(cc.ep);

        int maxDepth2 = maxTotalDepth - (int)path1.size();
        std::vector<int> path2;
        for (int d2 = 0; d2 <= maxDepth2; d2++) {
            path2.clear();
            if (searchPhase2(t, cp, ep, sp, d2, -1, path2, cancel, progress)) {
                outPhase2 = std::move(path2);
                return true;
            }
            if (cancel && cancel->load(std::memory_order_relaxed)) return false;
        }
        return false;
    }

    for (int m = 0; m < kNumMoves; m++) {
        if (!moveAllowedPrune(m, lastMove)) continue;

        t.applyMove(cc, m);
        path1.push_back(m);

        if (searchPhase1(t, cc, depth - 1, m, path1, outPhase2, maxTotalDepth, cancel, progress)) return true;

        path1.pop_back();
        t.applyMove(cc, t.invMove[m]);
    }
    return false;
}

} // namespace

bool Solver::moveAllowed(int move, int lastMove) {
    if (lastMove < 0) return true;
    int face = move / 3;
    int lastFace = lastMove / 3;
    if (face == lastFace) return false;
    if (face / 2 == lastFace / 2 && face < lastFace) return false;
    return true;
}

std::vector<Move> Solver::solve(Cube& cube) {
    return solve(cube, nullptr, nullptr);
}

std::vector<Move> Solver::solve(Cube& cube, std::atomic_bool* cancel, SolverProgress* progress) {
    if (cube.isSolved()) return {};
    if (!cube.isSolvable()) return {};
    if (cancel && cancel->load(std::memory_order_relaxed)) return {};

    if (progress) {
        progress->nodes.store(0, std::memory_order_relaxed);
        progress->depth.store(-1, std::memory_order_relaxed); // table build stage
    }

    // Ensure tables exist (built once per process).
    const Tables& t = tables();

    if (progress) {
        progress->depth.store(0, std::memory_order_relaxed);
    }

    CubieCube start = toCubie(cube);

    // Kociemba typically needs phase1 depth <= 12 and total solution <= 31 (FTM).
    constexpr int kMaxPhase1 = 12;
    constexpr int kMaxTotal = 31;

    std::vector<int> path1;
    std::vector<int> path2;

    for (int d1 = 0; d1 <= kMaxPhase1; d1++) {
        if (cancel && cancel->load(std::memory_order_relaxed)) return {};
        if (progress) progress->depth.store(d1, std::memory_order_relaxed);

        path1.clear();
        path2.clear();
        CubieCube cc = start;

        if (searchPhase1(t, cc, d1, -1, path1, path2, kMaxTotal, cancel, progress)) {
            std::vector<Move> solution;
            solution.reserve(path1.size() + path2.size());
            for (int m : path1) solution.push_back(static_cast<Move>(m));
            for (int m : path2) solution.push_back(static_cast<Move>(m));
            return solution;
        }
    }

    return {};
}
