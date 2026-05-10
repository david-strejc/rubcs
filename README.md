# rubcs

Simple 3D Rubik's Cube viewer with mouse/keyboard controls and an instant rewind solver.

The solver records legal moves as they happen, braid-normalizes opposite faces that
commute, compresses same-face turns, and solves by replaying the inverse trail.
Imported raw states are still validated, but only states reached through the app's
move API have a rewind trail.

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
