#pragma once
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <string>
#include <vector>
#include <queue>
#include <functional>
#include <future>
#include <chrono>
#include <atomic>
#include "cube.h"
#include "solver.h"
#include "font.h"

struct MoveAnimation {
    Move move = static_cast<Move>(255); // 255 = no animation
    float elapsed = 0.0f;
    float duration = 0.3f;
};

class Renderer {
public:
    bool init(int width, int height, const std::string& title);
    void run(Cube& cube, std::function<std::vector<Move>(Cube&, std::atomic_bool*, SolverProgress*)> solveFunc);
    void cleanup();

    // GLFW callbacks need access
    friend void keyCallback(GLFWwindow*, int, int, int, int);
    friend void mouseButtonCallback(GLFWwindow*, int, int, int);
    friend void cursorPosCallback(GLFWwindow*, double, double);
    friend void scrollCallback(GLFWwindow*, double, double);
    friend void framebufferSizeCallback(GLFWwindow*, int, int);

private:
    GLFWwindow* window_ = nullptr;
    int width_, height_;

    // Visual-only cube levitation (kept consistent across picking/rendering and frozen during drags).
    float levY_ = 0.0f;

    // Shaders
    GLuint cubeShader_ = 0;
    GLuint bgShader_ = 0;

    // Geometry
    GLuint cubeVAO_ = 0, cubeVBO_ = 0;
    GLuint bgVAO_ = 0, bgVBO_ = 0;

    // Camera (arcball)
    float camDist_ = 8.0f;
    float camYaw_ = 35.0f;
    float camPitch_ = 25.0f;
    bool rightDragging_ = false;
    double lastMouseX_ = 0, lastMouseY_ = 0;

    // Mouse drag for face moves
    bool leftDragging_ = false;
    double dragStartX_ = 0, dragStartY_ = 0;
    glm::vec3 dragStartWorld_;
    int dragFace_ = -1;
    glm::vec3 dragHitNormal_;

    // Animation
    MoveAnimation currentAnim_;
    std::queue<Move> moveQueue_;

    // HUD
    Font font_;
    Button btnScramble_;
    Button btnSolve_;
    Button btnReset_;
    double mouseX_ = 0, mouseY_ = 0;
    std::string statusText_;

    // Solve function (runs on a background thread to keep UI responsive).
    std::function<std::vector<Move>(Cube&, std::atomic_bool*, SolverProgress*)> solveFunc_;

    // Async solve (keeps UI responsive for harder scrambles).
    std::future<std::vector<Move>> solveFuture_;
    std::atomic_bool solveCancel_{false};
    SolverProgress solveProgress_{};
    double solveStartTime_ = 0.0;
    std::array<Color, 54> solveStartState_{}; // used to discard stale results if cube changes

    // Methods
    GLuint loadShader(const std::string& vertPath, const std::string& fragPath);
    void buildCubeGeometry();
    void buildBgGeometry();

    void renderBackground();
    void renderCube(Cube& cube, float time);
    void renderCubie(int x, int y, int z, Cube& cube, float animAngle, int animAxis, int animLayer);
    void renderHUD(Cube& cube);
    void renderButton(const Button& btn);

    glm::vec3 getCubieColor(Cube& cube, int x, int y, int z, int face);
    glm::mat4 getViewMatrix() const;
    glm::mat4 getProjectionMatrix() const;
    glm::vec3 getCameraPos() const;

    // Input
    void processKeyboard(Cube& cube, int key, int action, int mods);
    void processMouseButton(int button, int action, double xpos, double ypos, Cube& cube);
    void processMouseMove(double xpos, double ypos, Cube& cube);
    void processScroll(double yoffset);

    bool handleButtonClick(double x, double y, Cube& cube);
    void requestSolve(Cube& cube);
    void requestCancelSolve();

    // Ray casting
    glm::vec3 screenToWorldRay(double sx, double sy);
    bool raycastCube(const glm::vec3& origin, const glm::vec3& dir,
                     glm::vec3& hitPoint, glm::vec3& hitNormal, int& hitFace);
    Move determineMoveFromDrag(const glm::vec3& start, const glm::vec3& end,
                               int face, const glm::vec3& normal);

    void queueMove(Move m, Cube& cube);
};
