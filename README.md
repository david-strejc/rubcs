# rubcs

Simple 3D Rubik's Cube viewer with mouse/keyboard controls and a state-based solver.

The solver reads the cube state directly. It expands a frontier from the current
cube and another from the solved cube, then stitches the two move paths when their
states meet. It does not use move recording or replay history.

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
