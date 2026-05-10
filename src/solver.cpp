#include "solver.h"

#include <array>
#include <string>
#include <unordered_map>

namespace {

constexpr int kMoves = static_cast<int>(Move::COUNT);
constexpr int kHalfDepth = 5;

struct Node {
    std::array<Color, 54> state{};
    std::vector<Move> path;
    int lastFace = -1;
};

std::string keyOf(const std::array<Color, 54>& state) {
    std::string key(state.size(), '\0');
    for (size_t i = 0; i < state.size(); i++) key[i] = static_cast<char>(state[i]);
    return key;
}

int faceOf(Move move) {
    return static_cast<int>(move) / 3;
}

bool allowed(Move move, int lastFace) {
    return faceOf(move) != lastFace;
}

std::array<Color, 54> moved(const std::array<Color, 54>& state, Move move) {
    Cube cube;
    cube.setState(state);
    cube.applyMove(move);
    return cube.getState();
}

std::vector<Move> inverted(const std::vector<Move>& path) {
    std::vector<Move> out;
    out.reserve(path.size());
    for (auto it = path.rbegin(); it != path.rend(); ++it) out.push_back(Cube::inverseMove(*it));
    return out;
}

std::vector<Move> joined(const std::vector<Move>& fromStart, const std::vector<Move>& fromSolved) {
    auto tail = inverted(fromSolved);
    std::vector<Move> out = fromStart;
    out.insert(out.end(), tail.begin(), tail.end());
    return out;
}

bool expand(std::vector<Node>& frontier,
            std::unordered_map<std::string, std::vector<Move>>& own,
            const std::unordered_map<std::string, std::vector<Move>>& other,
            bool startSide,
            std::vector<Move>& solution,
            std::atomic_bool* cancel,
            SolverProgress* progress) {
    std::vector<Node> next;
    next.reserve(frontier.size() * 12);
    for (const auto& node : frontier) {
        if (cancel && cancel->load(std::memory_order_relaxed)) return false;
        for (int i = 0; i < kMoves; i++) {
            Move move = static_cast<Move>(i);
            if (!allowed(move, node.lastFace)) continue;

            Node child;
            child.state = moved(node.state, move);
            child.path = node.path;
            child.path.push_back(move);
            child.lastFace = faceOf(move);

            std::string key = keyOf(child.state);
            if (own.find(key) != own.end()) continue;
            if (progress) progress->nodes.fetch_add(1, std::memory_order_relaxed);

            auto meet = other.find(key);
            if (meet != other.end()) {
                solution = startSide ? joined(child.path, meet->second) : joined(meet->second, child.path);
                return true;
            }

            own.emplace(key, child.path);
            next.push_back(std::move(child));
        }
    }
    frontier = std::move(next);
    return false;
}

} // namespace

std::vector<Move> Solver::solve(Cube& cube) {
    return solve(cube, nullptr, nullptr);
}

std::vector<Move> Solver::solve(Cube& cube, std::atomic_bool* cancel, SolverProgress* progress) {
    if (progress) {
        progress->nodes.store(0, std::memory_order_relaxed);
        progress->depth.store(0, std::memory_order_relaxed);
    }
    if (cube.isSolved() || !cube.isSolvable() || (cancel && cancel->load(std::memory_order_relaxed))) return {};

    Cube solved;
    solved.reset();

    std::unordered_map<std::string, std::vector<Move>> startSeen;
    std::unordered_map<std::string, std::vector<Move>> solvedSeen;
    std::vector<Node> startFrontier = {{cube.getState(), {}, -1}};
    std::vector<Node> solvedFrontier = {{solved.getState(), {}, -1}};
    startSeen.emplace(keyOf(cube.getState()), std::vector<Move>{});
    solvedSeen.emplace(keyOf(solved.getState()), std::vector<Move>{});

    std::vector<Move> solution;
    for (int depth = 0; depth < kHalfDepth; depth++) {
        if (progress) progress->depth.store(depth * 2, std::memory_order_relaxed);
        if (expand(startFrontier, startSeen, solvedSeen, true, solution, cancel, progress)) return solution;
        if (progress) progress->depth.store(depth * 2 + 1, std::memory_order_relaxed);
        if (expand(solvedFrontier, solvedSeen, startSeen, false, solution, cancel, progress)) return solution;
    }
    return {};
}
