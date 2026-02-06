# TODO

- [x] Reproduce Rubik's Cube rotation bug (colors/stickers appear to swap randomly during/after rotation).
- [x] Identify root cause in cube state vs render/animation sync.
- [x] Fix rotation logic so sticker colors move consistently like a real cube (piece permutation + orientation).
- [x] Add test harness (CTest) and unit tests for core logic (cube moves + solver).
- [x] Add cube rotation correctness tests (per-move permutation vs physical model).
- [x] Add cube invariants tests (color counts, move inverse, move^4 identity, corner/edge validity).
- [x] Add solver tests (solves known scrambles, does not mutate input cube, heuristic sane on solved state).
- [x] Fix mirrored HUD text rendering (font bitmap bit order).
- [x] Make startup robust when `DISPLAY` is unset/empty/invalid (avoid instant exit on some shells).
- [x] Make Solve non-blocking and visible in UI (background thread + status/progress + cancel).
- [x] Add `Cube::isSolvable()` and validate after Scramble / before Solve.
- [x] Fix mouse drag move selection nondeterminism (freeze levitation during drag + add diagonal dead-zone).
- [x] Implement Kociemba two-phase solver (move/prune tables + phase1/phase2 search).
- [x] Update/extend solver tests for Kociemba (solve 20-move scramble, <=31 moves).
- [x] Polish Solve UI text for table-building stage (avoid showing negative depth).
- [ ] Create public GitHub repository and push code (`gh repo create ... --public --push`).
- [ ] Verify manually (rotate all faces, repeated sequences) and update docs if needed.
