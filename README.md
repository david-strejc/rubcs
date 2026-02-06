# rubcs

Simple 3D Rubik's Cube viewer with mouse/keyboard controls and an auto-solver.

The solver is a Kociemba-style two-phase solver. The first solve builds move/pruning
tables once per process; after that, solves should be fast.

## Build

Dependencies (Ubuntu-ish names):
- CMake
- OpenGL
- GLFW 3
- GLEW
- GLM (header-only)

```sh
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
./build/rubcs
```

## Tests

```sh
ctest --test-dir build --output-on-failure
```

## Controls

- `U/D/L/R/F/B` rotate face clockwise
- `Shift + key` rotate face counter-clockwise
- `Space` scramble
- `Enter` solve
- `Backspace` reset
- RMB drag: rotate camera
- LMB drag: rotate face
- Scroll: zoom
- `Esc` quit

