#include "renderer.h"
#include "solver.h"
#include <iostream>
#include <memory>

int main() {
    std::cout << "=== Rubik's Cube 3D ===\n";
    std::cout << "Controls:\n";
    std::cout << "  U/D/L/R/F/B     - rotate face clockwise\n";
    std::cout << "  Shift + key      - rotate face counter-clockwise\n";
    std::cout << "  Space            - scramble\n";
    std::cout << "  Enter            - auto-solve (Kociemba 2-phase)\n";
    std::cout << "  Backspace        - reset\n";
    std::cout << "  Right mouse drag - rotate camera\n";
    std::cout << "  Left mouse drag  - rotate face (drag on cube)\n";
    std::cout << "  Scroll           - zoom\n";
    std::cout << "  Escape           - quit\n\n";

    Solver solver;

    // Create cube
    Cube cube;

    // Create renderer
    Renderer renderer;
    if (!renderer.init(1280, 900, "Rubik's Cube 3D")) {
        std::cerr << "Failed to initialize renderer\n";
        return 1;
    }

    // Run main loop
    auto solveFunc = [&solver](Cube& c, std::atomic_bool* cancel, SolverProgress* progress) -> std::vector<Move> {
        return solver.solve(c, cancel, progress);
    };
    renderer.run(cube, solveFunc);

    renderer.cleanup();
    return 0;
}
