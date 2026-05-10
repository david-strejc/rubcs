#include "solver.h"

std::vector<Move> Solver::solve(Cube& cube) {
    return solve(cube, nullptr, nullptr);
}

std::vector<Move> Solver::solve(Cube& cube, std::atomic_bool* cancel, SolverProgress* progress) {
    if (progress) {
        progress->nodes.store(0, std::memory_order_relaxed);
        progress->depth.store(0, std::memory_order_relaxed);
    }
    if (cube.isSolved() || !cube.isSolvable() || (cancel && cancel->load(std::memory_order_relaxed))) {
        return {};
    }

    auto solution = cube.rewindSolution();
    if (progress) {
        progress->nodes.store(solution.size(), std::memory_order_relaxed);
        progress->depth.store(static_cast<int>(solution.size()), std::memory_order_relaxed);
    }
    return solution;
}
