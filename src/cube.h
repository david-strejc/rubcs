#pragma once
#include <array>
#include <vector>
#include <string>
#include <random>
#include <functional>

enum class Color : uint8_t {
    White,   // U - top
    Yellow,  // D - bottom
    Red,     // F - front
    Orange,  // B - back
    Green,   // L - left
    Blue     // R - right
};

// Standard Rubik's cube moves
enum class Move : uint8_t {
    U, Up, U2,   // Up face
    D, Dp, D2,   // Down face
    L, Lp, L2,   // Left face
    R, Rp, R2,   // Right face
    F, Fp, F2,   // Front face
    B, Bp, B2,   // Back face
    COUNT
};

// Face indices
enum Face : int { FACE_U = 0, FACE_D, FACE_L, FACE_R, FACE_F, FACE_B };

class Cube {
public:
    Cube();

    void reset();
    void applyMove(Move m);
    void scramble(int numMoves = 20);
    bool isSolved() const;
    bool isSolvable() const;

    Color getFacelet(int face, int index) const { return state_[face * 9 + index]; }
    const std::array<Color, 54>& getState() const { return state_; }

    // For solver - get/set raw state
    void setState(const std::array<Color, 54>& s) { state_ = s; }

    static Move inverseMove(Move m);
    static std::string moveToString(Move m);
    static const char* colorName(Color c);

    // Map a cubie surface coordinate (x,y,z in {-1,0,1}) to a facelet index [0..8] on `face`.
    // Returns -1 if the coordinate is not on that face.
    // Face orientation matches the solver's facelet conventions and the renderer's cube net.
    static int faceletIndexFor(int face, int x, int y, int z);

    // Corner and edge indexing for solver
    // Corner cubies: URF, UFL, ULB, UBR, DFR, DLF, DBL, DRB
    // Edge cubies: UR, UF, UL, UB, DR, DF, DL, DB, FR, FL, BL, BR
    int getCornerOrientation(int corner) const;
    int getEdgeOrientation(int edge) const;
    int getCornerPermutation(int corner) const;
    int getEdgePermutation(int edge) const;

    // Coordinate extraction for Kociemba solver
    int cornerOrientationCoord() const;
    int edgeOrientationCoord() const;
    int udSliceCoord() const;
    int cornerPermutationCoord() const;
    int phase2EdgePermutationCoord() const;
    int udSlicePermutationCoord() const;

private:
    std::array<Color, 54> state_;  // 6 faces * 9 facelets

    void rotateFaceCW(int face);
    void rotateFaceCCW(int face);

    // Cycle helpers
    void cycle4(int a, int b, int c, int d);
};
