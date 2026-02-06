#pragma once
#include "cube.h"
#include <atomic>
#include <cstdint>
#include <vector>

struct SolverProgress {
    std::atomic<uint64_t> nodes{0};
    std::atomic<int> depth{0};
};

class Solver {
public:
    std::vector<Move> solve(Cube& cube);
    std::vector<Move> solve(Cube& cube, std::atomic_bool* cancel, SolverProgress* progress = nullptr);

private:
    static bool moveAllowed(int move, int lastMove);
};
