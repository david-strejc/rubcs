#include "renderer.h"
#include <fstream>
#include <sstream>
#include <iostream>
#include <cmath>
#include <algorithm>
#include <cstdlib>
#include <cctype>
#include <unistd.h>

// ============================================================
// Color definitions (RGB)
// ============================================================
static glm::vec3 faceColor(Color c) {
    switch (c) {
        case Color::White:  return {0.95f, 0.95f, 0.95f};
        case Color::Yellow: return {1.0f, 0.85f, 0.0f};
        case Color::Red:    return {0.85f, 0.12f, 0.08f};
        case Color::Orange: return {1.0f, 0.55f, 0.0f};
        case Color::Green:  return {0.0f, 0.62f, 0.12f};
        case Color::Blue:   return {0.0f, 0.32f, 0.73f};
    }
    return {0.1f, 0.1f, 0.1f};
}

static const glm::vec3 BLACK = {0.05f, 0.05f, 0.05f};

// ============================================================
// Shader loading
// ============================================================
GLuint Renderer::loadShader(const std::string& vertPath, const std::string& fragPath) {
    auto readFile = [](const std::string& path) -> std::string {
        std::ifstream f(path);
        if (!f.is_open()) {
            std::cerr << "Failed to open shader: " << path << std::endl;
            return "";
        }
        std::stringstream ss;
        ss << f.rdbuf();
        return ss.str();
    };

    std::string vertSrc = readFile(vertPath);
    std::string fragSrc = readFile(fragPath);
    const char* vSrc = vertSrc.c_str();
    const char* fSrc = fragSrc.c_str();

    GLuint vs = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vs, 1, &vSrc, nullptr);
    glCompileShader(vs);
    int ok; char log[512];
    glGetShaderiv(vs, GL_COMPILE_STATUS, &ok);
    if (!ok) { glGetShaderInfoLog(vs, 512, nullptr, log); std::cerr << "VS error: " << log << std::endl; }

    GLuint fs = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fs, 1, &fSrc, nullptr);
    glCompileShader(fs);
    glGetShaderiv(fs, GL_COMPILE_STATUS, &ok);
    if (!ok) { glGetShaderInfoLog(fs, 512, nullptr, log); std::cerr << "FS error: " << log << std::endl; }

    GLuint prog = glCreateProgram();
    glAttachShader(prog, vs);
    glAttachShader(prog, fs);
    glLinkProgram(prog);
    glGetProgramiv(prog, GL_LINK_STATUS, &ok);
    if (!ok) { glGetProgramInfoLog(prog, 512, nullptr, log); std::cerr << "Link error: " << log << std::endl; }

    glDeleteShader(vs);
    glDeleteShader(fs);
    return prog;
}

// ============================================================
// Geometry building
// ============================================================

// A single face (quad as 2 triangles) with position, normal, color
static void addQuad(std::vector<float>& verts,
                    const glm::vec3& p0, const glm::vec3& p1,
                    const glm::vec3& p2, const glm::vec3& p3,
                    const glm::vec3& normal, const glm::vec3& color) {
    auto addVert = [&](const glm::vec3& p) {
        verts.push_back(p.x); verts.push_back(p.y); verts.push_back(p.z);
        verts.push_back(normal.x); verts.push_back(normal.y); verts.push_back(normal.z);
        verts.push_back(color.x); verts.push_back(color.y); verts.push_back(color.z);
    };
    addVert(p0); addVert(p1); addVert(p2);
    addVert(p0); addVert(p2); addVert(p3);
}

void Renderer::buildCubeGeometry() {
    // Just set up VAO/VBO - we'll fill data each frame
    glGenVertexArrays(1, &cubeVAO_);
    glGenBuffers(1, &cubeVBO_);
    glBindVertexArray(cubeVAO_);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO_);
    // Position
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    // Normal
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    // Color
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 9 * sizeof(float), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);
}

void Renderer::buildBgGeometry() {
    float quad[] = { -1, -1, 1, -1, 1, 1, -1, -1, 1, 1, -1, 1 };
    glGenVertexArrays(1, &bgVAO_);
    glGenBuffers(1, &bgVBO_);
    glBindVertexArray(bgVAO_);
    glBindBuffer(GL_ARRAY_BUFFER, bgVBO_);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quad), quad, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
}

// ============================================================
// Cubie rendering
// ============================================================

// Map cubie grid position (x,y,z in -1..1) to the color on a specific external face
glm::vec3 Renderer::getCubieColor(Cube& cube, int x, int y, int z, int faceDir) {
    // faceDir: 0=+Y(U), 1=-Y(D), 2=-X(L), 3=+X(R), 4=+Z(F), 5=-Z(B)
    // x,y,z in {-1, 0, 1}
    int face = -1;
    switch (faceDir) {
    case 0: face = FACE_U; break;
    case 1: face = FACE_D; break;
    case 2: face = FACE_L; break;
    case 3: face = FACE_R; break;
    case 4: face = FACE_F; break;
    case 5: face = FACE_B; break;
    default: return BLACK;
    }

    int idx = Cube::faceletIndexFor(face, x, y, z);
    if (idx < 0) return BLACK; // internal face, no sticker
    return faceColor(cube.getFacelet(face, idx));
}

// Check if a cubie at (x,y,z) belongs to the animated layer
static bool isInAnimLayer(int x, int y, int z, int animAxis, int animLayer) {
    if (animAxis < 0) return false;
    switch (animAxis) {
    case 0: return x == animLayer; // X axis (L/R)
    case 1: return y == animLayer; // Y axis (U/D)
    case 2: return z == animLayer; // Z axis (F/B)
    }
    return false;
}

// Get animation axis and layer for a move
static void getMoveAxisLayer(Move m, int& axis, int& layer, float& angle) {
    int idx = static_cast<int>(m);
    int face = idx / 3;
    int type = idx % 3; // 0=CW, 1=CCW, 2=double

    float baseAngle = glm::radians(90.0f);
    if (type == 1) baseAngle = -baseAngle;
    if (type == 2) baseAngle = glm::radians(180.0f);

    switch (face) {
    case 0: axis = 1; layer =  1; angle = -baseAngle; break; // U
    case 1: axis = 1; layer = -1; angle =  baseAngle; break; // D
    case 2: axis = 0; layer = -1; angle =  baseAngle; break; // L
    case 3: axis = 0; layer =  1; angle = -baseAngle; break; // R
    case 4: axis = 2; layer =  1; angle = -baseAngle; break; // F
    case 5: axis = 2; layer = -1; angle =  baseAngle; break; // B
    }
}

void Renderer::renderCubie(int x, int y, int z, Cube& cube,
                           float animAngle, int animAxis, int animLayer) {
    // Each cubie is centered at (x, y, z) with half-size slightly less than 0.5
    float hs = 0.47f;      // half-size of cubie body
    float ss = 0.42f;      // half-size of sticker
    float sOff = 0.471f;   // sticker offset from center (slightly above surface)

    glm::vec3 center(x, y, z);
    // If animating, compute rotation
    glm::mat4 model(1.0f);

    // Levitation
    model = glm::translate(model, glm::vec3(0, levY_, 0));

    // Idle rotation when no animation/moves
    if (moveQueue_.empty() && currentAnim_.move == static_cast<Move>(255)) {
        // idle state - no animation running
    }

    if (isInAnimLayer(x, y, z, animAxis, animLayer) && animAngle != 0.0f) {
        glm::vec3 rotAxis(0);
        rotAxis[animAxis] = 1.0f;
        model = model * glm::rotate(glm::mat4(1.0f), animAngle, rotAxis);
    }

    std::vector<float> verts;
    verts.reserve(9 * 6 * 6 * 2); // rough estimate

    glm::vec3 corners[8] = {
        center + glm::vec3(-hs, -hs, -hs),
        center + glm::vec3( hs, -hs, -hs),
        center + glm::vec3( hs,  hs, -hs),
        center + glm::vec3(-hs,  hs, -hs),
        center + glm::vec3(-hs, -hs,  hs),
        center + glm::vec3( hs, -hs,  hs),
        center + glm::vec3( hs,  hs,  hs),
        center + glm::vec3(-hs,  hs,  hs),
    };

    // 6 faces of the cubie body (black plastic)
    // +Y (top)
    addQuad(verts, corners[3], corners[2], corners[6], corners[7], {0,1,0}, BLACK);
    // -Y (bottom)
    addQuad(verts, corners[4], corners[5], corners[1], corners[0], {0,-1,0}, BLACK);
    // +X (right)
    addQuad(verts, corners[1], corners[5], corners[6], corners[2], {1,0,0}, BLACK);
    // -X (left)
    addQuad(verts, corners[4], corners[0], corners[3], corners[7], {-1,0,0}, BLACK);
    // +Z (front)
    addQuad(verts, corners[5], corners[4], corners[7], corners[6], {0,0,1}, BLACK);
    // -Z (back)
    addQuad(verts, corners[0], corners[1], corners[2], corners[3], {0,0,-1}, BLACK);

    // Stickers on external faces only
    struct FaceDef { int dir; glm::vec3 normal; glm::vec3 right; glm::vec3 up; };
    FaceDef faces[] = {
        {0, {0,1,0}, {1,0,0}, {0,0,-1}},   // +Y (U)
        {1, {0,-1,0}, {1,0,0}, {0,0,1}},    // -Y (D)
        {2, {-1,0,0}, {0,0,-1}, {0,1,0}},   // -X (L)
        {3, {1,0,0}, {0,0,1}, {0,1,0}},     // +X (R)
        {4, {0,0,1}, {1,0,0}, {0,1,0}},     // +Z (F)
        {5, {0,0,-1}, {-1,0,0}, {0,1,0}},   // -Z (B)
    };

    for (auto& fd : faces) {
        glm::vec3 color = getCubieColor(cube, x, y, z, fd.dir);
        if (color == BLACK) continue; // internal face, no sticker

        glm::vec3 sCenter = center + fd.normal * sOff;
        glm::vec3 p0 = sCenter - fd.right * ss - fd.up * ss;
        glm::vec3 p1 = sCenter + fd.right * ss - fd.up * ss;
        glm::vec3 p2 = sCenter + fd.right * ss + fd.up * ss;
        glm::vec3 p3 = sCenter - fd.right * ss + fd.up * ss;
        addQuad(verts, p0, p1, p2, p3, fd.normal, color);
    }

    // Upload and draw
    glUseProgram(cubeShader_);
    glUniformMatrix4fv(glGetUniformLocation(cubeShader_, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix4fv(glGetUniformLocation(cubeShader_, "view"), 1, GL_FALSE, glm::value_ptr(getViewMatrix()));
    glUniformMatrix4fv(glGetUniformLocation(cubeShader_, "projection"), 1, GL_FALSE, glm::value_ptr(getProjectionMatrix()));
    glUniform3fv(glGetUniformLocation(cubeShader_, "lightPos"), 1, glm::value_ptr(glm::vec3(5, 8, 6)));
    glUniform3fv(glGetUniformLocation(cubeShader_, "viewPos"), 1, glm::value_ptr(getCameraPos()));

    glBindVertexArray(cubeVAO_);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO_);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, (GLsizei)(verts.size() / 9));
    glBindVertexArray(0);
}

void Renderer::renderCube(Cube& cube, float time) {
    int animAxis = -1, animLayer = 0;
    float animAngle = 0.0f;

    if (currentAnim_.move != static_cast<Move>(255)) {
        int axis; int layer; float targetAngle;
        getMoveAxisLayer(currentAnim_.move, axis, layer, targetAngle);
        animAxis = axis;
        animLayer = layer;

        // Ease in-out cubic
        float t = currentAnim_.elapsed / currentAnim_.duration;
        t = std::clamp(t, 0.0f, 1.0f);
        float eased = t < 0.5f ? 4*t*t*t : 1 - powf(-2*t+2, 3)/2;
        animAngle = targetAngle * eased;
    }

    for (int x = -1; x <= 1; x++)
        for (int y = -1; y <= 1; y++)
            for (int z = -1; z <= 1; z++) {
                if (x == 0 && y == 0 && z == 0) continue; // skip center
                renderCubie(x, y, z, cube, animAngle, animAxis, animLayer);
            }
}

void Renderer::renderBackground() {
    glDisable(GL_DEPTH_TEST);
    glUseProgram(bgShader_);
    glBindVertexArray(bgVAO_);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);
}

// ============================================================
// Camera
// ============================================================
glm::vec3 Renderer::getCameraPos() const {
    float yawR = glm::radians(camYaw_);
    float pitchR = glm::radians(camPitch_);
    return glm::vec3(
        camDist_ * cosf(pitchR) * sinf(yawR),
        camDist_ * sinf(pitchR),
        camDist_ * cosf(pitchR) * cosf(yawR)
    );
}

glm::mat4 Renderer::getViewMatrix() const {
    return glm::lookAt(getCameraPos(), glm::vec3(0, levY_, 0), glm::vec3(0, 1, 0));
}

glm::mat4 Renderer::getProjectionMatrix() const {
    return glm::perspective(glm::radians(45.0f), (float)width_ / height_, 0.1f, 100.0f);
}

// ============================================================
// Ray casting for mouse picking
// ============================================================
glm::vec3 Renderer::screenToWorldRay(double sx, double sy) {
    float x = (2.0f * sx / width_ - 1.0f);
    float y = (1.0f - 2.0f * sy / height_);
    glm::vec4 clipNear(x, y, -1.0f, 1.0f);
    glm::mat4 invVP = glm::inverse(getProjectionMatrix() * getViewMatrix());
    glm::vec4 worldNear = invVP * clipNear;
    worldNear /= worldNear.w;
    glm::vec3 camPos = getCameraPos();
    return glm::normalize(glm::vec3(worldNear) - camPos);
}

bool Renderer::raycastCube(const glm::vec3& origin, const glm::vec3& dir,
                           glm::vec3& hitPoint, glm::vec3& hitNormal, int& hitFace) {
    // Test ray against the 6 outer planes of the 3x3x3 cube
    // The cube spans from -1.5 to 1.5 on each axis
    struct Plane { glm::vec3 normal; float d; int face; };
    Plane planes[] = {
        {{ 0, 1, 0},  1.5f + levY_, 0},  // U (+Y)
        {{ 0,-1, 0},  1.5f - levY_, 1},  // D (-Y)
        {{-1, 0, 0},  1.5f, 2},         // L (-X)
        {{ 1, 0, 0},  1.5f, 3},         // R (+X)
        {{ 0, 0, 1},  1.5f, 4},         // F (+Z)
        {{ 0, 0,-1},  1.5f, 5},         // B (-Z)
    };

    float minT = 1e9f;
    bool hit = false;
    for (auto& pl : planes) {
        float denom = glm::dot(pl.normal, dir);
        if (fabsf(denom) < 1e-6f) continue;
        float t = -(glm::dot(pl.normal, origin) + pl.d) / denom;
        // Fix: plane equation is normal·(P - planePoint) = 0
        // For +Y at y=1.5: normal=(0,1,0), point on plane: (0, 1.5+levY, 0)
        // normal·P = 1.5+levY => normal·origin + t*normal·dir = 1.5+levY
        glm::vec3 planeCenter(0, levY_, 0);
        float planeDist;
        if (pl.face == 0) planeDist = 1.5f;
        else if (pl.face == 1) planeDist = 1.5f;
        else planeDist = 1.5f;

        glm::vec3 planePoint = planeCenter + pl.normal * planeDist;
        t = glm::dot(planePoint - origin, pl.normal) / denom;

        if (t < 0 || t > minT) continue;
        glm::vec3 p = origin + dir * t;
        glm::vec3 local = p - glm::vec3(0, levY_, 0);

        // Check bounds on the other 2 axes
        bool inBounds = true;
        for (int i = 0; i < 3; i++) {
            if (fabsf(pl.normal[i]) > 0.5f) continue;
            if (local[i] < -1.5f || local[i] > 1.5f) { inBounds = false; break; }
        }
        if (!inBounds) continue;

        minT = t;
        hitPoint = p;
        hitNormal = pl.normal;
        hitFace = pl.face;
        hit = true;
    }
    return hit;
}

Move Renderer::determineMoveFromDrag(const glm::vec3& start, const glm::vec3& end,
                                      int face, const glm::vec3& normal) {
    glm::vec3 localStart = start - glm::vec3(0, levY_, 0);
    glm::vec3 localEnd = end - glm::vec3(0, levY_, 0);
    glm::vec3 drag = localEnd - localStart;

    // Determine which cubie row/column the drag started in
    // and which direction the drag goes
    int layer; // -1, 0, 1

    // For each face, we need to determine which axis the drag is along
    // and which layer is affected
    switch (face) {
    case 0: { // U face (+Y), drag in XZ plane
        float dx = drag.x, dz = drag.z;
        if (fabsf(dx) > fabsf(dz) * 1.2f) {
            // Dragging along X
            layer = (int)roundf(localStart.z);
            layer = std::clamp(layer, -1, 1);
            if (dx > 0) {
                if (layer == 1) return Move::F;
                if (layer == 0) return Move::COUNT; // middle slice, skip
                return Move::Bp;
            } else {
                if (layer == 1) return Move::Fp;
                if (layer == 0) return Move::COUNT;
                return Move::B;
            }
        } else if (fabsf(dz) > fabsf(dx) * 1.2f) {
            // Dragging along Z
            layer = (int)roundf(localStart.x);
            layer = std::clamp(layer, -1, 1);
            if (dz > 0) {
                if (layer == 1) return Move::Rp;
                if (layer == 0) return Move::COUNT;
                return Move::L;
            } else {
                if (layer == 1) return Move::R;
                if (layer == 0) return Move::COUNT;
                return Move::Lp;
            }
        } else {
            return Move::COUNT; // ambiguous diagonal drag
        }
    }
    case 1: { // D face (-Y), drag in XZ plane
        float dx = drag.x, dz = drag.z;
        if (fabsf(dx) > fabsf(dz) * 1.2f) {
            layer = (int)roundf(localStart.z);
            layer = std::clamp(layer, -1, 1);
            if (dx > 0) {
                if (layer == 1) return Move::Fp;
                if (layer == 0) return Move::COUNT;
                return Move::B;
            } else {
                if (layer == 1) return Move::F;
                if (layer == 0) return Move::COUNT;
                return Move::Bp;
            }
        } else if (fabsf(dz) > fabsf(dx) * 1.2f) {
            layer = (int)roundf(localStart.x);
            layer = std::clamp(layer, -1, 1);
            if (dz > 0) {
                if (layer == 1) return Move::R;
                if (layer == 0) return Move::COUNT;
                return Move::Lp;
            } else {
                if (layer == 1) return Move::Rp;
                if (layer == 0) return Move::COUNT;
                return Move::L;
            }
        } else {
            return Move::COUNT;
        }
    }
    case 4: { // F face (+Z), drag in XY plane
        float dx = drag.x, dy = drag.y;
        if (fabsf(dx) > fabsf(dy) * 1.2f) {
            layer = (int)roundf(localStart.y);
            layer = std::clamp(layer, -1, 1);
            if (dx > 0) {
                if (layer == 1) return Move::U;
                if (layer == 0) return Move::COUNT;
                return Move::Dp;
            } else {
                if (layer == 1) return Move::Up;
                if (layer == 0) return Move::COUNT;
                return Move::D;
            }
        } else if (fabsf(dy) > fabsf(dx) * 1.2f) {
            layer = (int)roundf(localStart.x);
            layer = std::clamp(layer, -1, 1);
            if (dy > 0) {
                if (layer == 1) return Move::Rp;
                if (layer == 0) return Move::COUNT;
                return Move::L;
            } else {
                if (layer == 1) return Move::R;
                if (layer == 0) return Move::COUNT;
                return Move::Lp;
            }
        } else {
            return Move::COUNT;
        }
    }
    case 5: { // B face (-Z), drag in XY plane
        float dx = drag.x, dy = drag.y;
        if (fabsf(dx) > fabsf(dy) * 1.2f) {
            layer = (int)roundf(localStart.y);
            layer = std::clamp(layer, -1, 1);
            if (dx > 0) {
                if (layer == 1) return Move::Up;
                if (layer == 0) return Move::COUNT;
                return Move::D;
            } else {
                if (layer == 1) return Move::U;
                if (layer == 0) return Move::COUNT;
                return Move::Dp;
            }
        } else if (fabsf(dy) > fabsf(dx) * 1.2f) {
            layer = (int)roundf(localStart.x);
            layer = std::clamp(layer, -1, 1);
            if (dy > 0) {
                if (layer == -1) return Move::Lp;
                if (layer == 0) return Move::COUNT;
                return Move::R;
            } else {
                if (layer == -1) return Move::L;
                if (layer == 0) return Move::COUNT;
                return Move::Rp;
            }
        } else {
            return Move::COUNT;
        }
    }
    case 2: { // L face (-X), drag in YZ plane
        float dy = drag.y, dz = drag.z;
        if (fabsf(dz) > fabsf(dy) * 1.2f) {
            layer = (int)roundf(localStart.y);
            layer = std::clamp(layer, -1, 1);
            if (dz > 0) {
                if (layer == 1) return Move::Up;
                if (layer == 0) return Move::COUNT;
                return Move::D;
            } else {
                if (layer == 1) return Move::U;
                if (layer == 0) return Move::COUNT;
                return Move::Dp;
            }
        } else if (fabsf(dy) > fabsf(dz) * 1.2f) {
            layer = (int)roundf(localStart.z);
            layer = std::clamp(layer, -1, 1);
            if (dy > 0) {
                if (layer == 1) return Move::F;
                if (layer == 0) return Move::COUNT;
                return Move::Bp;
            } else {
                if (layer == 1) return Move::Fp;
                if (layer == 0) return Move::COUNT;
                return Move::B;
            }
        } else {
            return Move::COUNT;
        }
    }
    case 3: { // R face (+X), drag in YZ plane
        float dy = drag.y, dz = drag.z;
        if (fabsf(dz) > fabsf(dy) * 1.2f) {
            layer = (int)roundf(localStart.y);
            layer = std::clamp(layer, -1, 1);
            if (dz > 0) {
                if (layer == 1) return Move::U;
                if (layer == 0) return Move::COUNT;
                return Move::Dp;
            } else {
                if (layer == 1) return Move::Up;
                if (layer == 0) return Move::COUNT;
                return Move::D;
            }
        } else if (fabsf(dy) > fabsf(dz) * 1.2f) {
            layer = (int)roundf(localStart.z);
            layer = std::clamp(layer, -1, 1);
            if (dy > 0) {
                if (layer == 1) return Move::Fp;
                if (layer == 0) return Move::COUNT;
                return Move::B;
            } else {
                if (layer == 1) return Move::F;
                if (layer == 0) return Move::COUNT;
                return Move::Bp;
            }
        } else {
            return Move::COUNT;
        }
    }
    }
    return Move::COUNT;
}

// ============================================================
// Input handling
// ============================================================
void Renderer::queueMove(Move m, Cube& cube) {
    if (m == Move::COUNT) return;
    if (solveFuture_.valid() &&
        solveFuture_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
        // Avoid mixing user input with an in-progress solve (solution would be stale anyway).
        statusText_ = "Solving... (press SOLVE again to cancel)";
        return;
    }
    moveQueue_.push(m);
}

void Renderer::processKeyboard(Cube& cube, int key, int action, int mods) {
    if (action != GLFW_PRESS) return;
    bool shift = (mods & GLFW_MOD_SHIFT) != 0;

    switch (key) {
    case GLFW_KEY_U: queueMove(shift ? Move::Up : Move::U, cube); break;
    case GLFW_KEY_D: queueMove(shift ? Move::Dp : Move::D, cube); break;
    case GLFW_KEY_L: queueMove(shift ? Move::Lp : Move::L, cube); break;
    case GLFW_KEY_R: queueMove(shift ? Move::Rp : Move::R, cube); break;
    case GLFW_KEY_F: queueMove(shift ? Move::Fp : Move::F, cube); break;
    case GLFW_KEY_B: queueMove(shift ? Move::Bp : Move::B, cube); break;
    case GLFW_KEY_SPACE:
        requestCancelSolve();
        cube.scramble(20);
        statusText_ = cube.isSolvable() ? "Scrambled" : "Scramble produced invalid state (reset)";
        if (!cube.isSolvable()) cube.reset();
        break;
    case GLFW_KEY_BACKSPACE:
        requestCancelSolve();
        cube.reset();
        // Clear any pending moves
        while (!moveQueue_.empty()) moveQueue_.pop();
        currentAnim_.move = static_cast<Move>(255);
        statusText_ = "";
        break;
    case GLFW_KEY_ENTER:
    case GLFW_KEY_KP_ENTER: {
        requestSolve(cube);
        break;
    }
    case GLFW_KEY_ESCAPE:
        glfwSetWindowShouldClose(window_, true);
        break;
    }
}

void Renderer::processMouseButton(int button, int action, double xpos, double ypos, Cube& cube) {
    if (button == GLFW_MOUSE_BUTTON_RIGHT) {
        if (action == GLFW_PRESS) {
            rightDragging_ = true;
            lastMouseX_ = xpos;
            lastMouseY_ = ypos;
        } else {
            rightDragging_ = false;
        }
    }
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        if (action == GLFW_PRESS) {
            // Freeze levitation while dragging to keep picking/mapping stable.
            levY_ = levY_;
            // Check buttons first
            if (handleButtonClick(xpos, ypos, cube)) return;
            // Try to hit the cube
            glm::vec3 ray = screenToWorldRay(xpos, ypos);
            glm::vec3 hitPt, hitNorm;
            int hitFace;
            if (raycastCube(getCameraPos(), ray, hitPt, hitNorm, hitFace)) {
                leftDragging_ = true;
                dragStartX_ = xpos;
                dragStartY_ = ypos;
                dragStartWorld_ = hitPt;
                dragFace_ = hitFace;
                dragHitNormal_ = hitNorm;
            }
        } else if (action == GLFW_RELEASE && leftDragging_) {
            leftDragging_ = false;
            // Determine end point
            glm::vec3 ray = screenToWorldRay(xpos, ypos);
            glm::vec3 hitPt, hitNorm;
            int hitFace;
            // Project onto same plane
            float denom = glm::dot(dragHitNormal_, ray);
            if (fabsf(denom) > 1e-6f) {
                float t = glm::dot(dragStartWorld_ - getCameraPos(), dragHitNormal_) / denom;
                // Actually compute properly:
                // plane: normal · (P - dragStartWorld_) = 0
                // P = camPos + t * ray
                // normal · (camPos + t*ray - dragStartWorld_) = 0
                t = glm::dot(dragStartWorld_ - getCameraPos(), dragHitNormal_) /
                    glm::dot(ray, dragHitNormal_);
                glm::vec3 endPt = getCameraPos() + ray * t;

                float dragDist = glm::length(endPt - dragStartWorld_);
                if (dragDist > 0.3f) { // minimum threshold
                    Move m = determineMoveFromDrag(dragStartWorld_, endPt, dragFace_, dragHitNormal_);
                    queueMove(m, cube);
                }
            }
        }
    }
}

void Renderer::processMouseMove(double xpos, double ypos, Cube& cube) {
    mouseX_ = xpos;
    mouseY_ = ypos;
    if (rightDragging_) {
        float dx = (float)(xpos - lastMouseX_);
        float dy = (float)(ypos - lastMouseY_);
        camYaw_ += dx * 0.3f;
        camPitch_ += dy * 0.3f;
        camPitch_ = std::clamp(camPitch_, -89.0f, 89.0f);
        lastMouseX_ = xpos;
        lastMouseY_ = ypos;
    }
}

void Renderer::processScroll(double yoffset) {
    camDist_ -= (float)yoffset * 0.5f;
    camDist_ = std::clamp(camDist_, 4.0f, 15.0f);
}

// ============================================================
// Init & Main loop
// ============================================================

// GLFW callback data
static Renderer* g_renderer = nullptr;
static Cube* g_cube = nullptr;

void keyCallback(GLFWwindow*, int key, int, int action, int mods) {
    g_renderer->processKeyboard(*g_cube, key, action, mods);
}
void mouseButtonCallback(GLFWwindow*, int button, int action, int mods) {
    double x, y;
    glfwGetCursorPos(g_renderer->window_, &x, &y);
    g_renderer->processMouseButton(button, action, x, y, *g_cube);
}
void cursorPosCallback(GLFWwindow*, double x, double y) {
    g_renderer->processMouseMove(x, y, *g_cube);
}
void scrollCallback(GLFWwindow*, double, double yoff) {
    g_renderer->processScroll(yoff);
}
void framebufferSizeCallback(GLFWwindow*, int w, int h) {
    g_renderer->width_ = w;
    g_renderer->height_ = h;
    glViewport(0, 0, w, h);
}

bool Renderer::init(int width, int height, const std::string& title) {
    width_ = width;
    height_ = height;

#if defined(__unix__) || defined(__APPLE__)
    // In some non-interactive shells DISPLAY may be missing, or users may set it as "0".
    // GLFW/X11 requires a valid DISPLAY like ":0" or "localhost:10.0".
    auto findDefaultXDisplay = []() -> std::string {
        // Probe the common X11 socket locations first.
        // This avoids hardcoding :0 on systems where the active display is :1, :2, etc.
        for (int i = 0; i <= 9; i++) {
            std::string sock = "/tmp/.X11-unix/X" + std::to_string(i);
            if (::access(sock.c_str(), F_OK) == 0) return ":" + std::to_string(i);
        }
        return ":0";
    };

    const char* disp = std::getenv("DISPLAY");
    if (!disp || disp[0] == '\0') {
        std::string d = findDefaultXDisplay();
        setenv("DISPLAY", d.c_str(), 1); // overwrite empty/unset
    } else {
        std::string s(disp);
        bool digitsOnly = !s.empty();
        for (unsigned char ch : s) {
            if (!std::isdigit(ch)) { digitsOnly = false; break; }
        }
        if (digitsOnly) {
            std::string normalized = ":" + s;
            setenv("DISPLAY", normalized.c_str(), 1);
        }
    }
#endif

    if (!glfwInit()) {
        const char* desc = nullptr;
        int err = glfwGetError(&desc);
        std::cerr << "Failed to init GLFW";
        if (err != GLFW_NO_ERROR) {
            std::cerr << " (error " << err;
            if (desc) std::cerr << ": " << desc;
            std::cerr << ")";
        }
        const char* disp = std::getenv("DISPLAY");
        std::cerr << " [DISPLAY=" << (disp ? disp : "<unset>") << "]\n";
        return false;
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_SAMPLES, 4); // MSAA

    window_ = glfwCreateWindow(width, height, title.c_str(), nullptr, nullptr);
    if (!window_) { std::cerr << "Failed to create window\n"; glfwTerminate(); return false; }
    glfwMakeContextCurrent(window_);
    glfwSwapInterval(1); // vsync

    glewExperimental = GL_TRUE;
    if (glewInit() != GLEW_OK) { std::cerr << "Failed to init GLEW\n"; return false; }

    glEnable(GL_DEPTH_TEST);
    glEnable(GL_MULTISAMPLE);

    // Load shaders
    cubeShader_ = loadShader("shaders/cube.vert", "shaders/cube.frag");
    bgShader_ = loadShader("shaders/bg.vert", "shaders/bg.frag");

    buildCubeGeometry();
    buildBgGeometry();

    // Init font and buttons
    font_.init();
    btnScramble_ = {0.02f, 0.92f, 0.12f, 0.05f, "SCRAMBLE",
                    {0.2f, 0.6f, 0.2f}, {0.3f, 0.8f, 0.3f}};
    btnSolve_ = {0.16f, 0.92f, 0.10f, 0.05f, "SOLVE",
                 {0.2f, 0.4f, 0.8f}, {0.3f, 0.5f, 1.0f}};
    btnReset_ = {0.28f, 0.92f, 0.10f, 0.05f, "RESET",
                 {0.7f, 0.2f, 0.2f}, {0.9f, 0.3f, 0.3f}};

    // Set callbacks
    g_renderer = this;
    glfwSetKeyCallback(window_, keyCallback);
    glfwSetMouseButtonCallback(window_, mouseButtonCallback);
    glfwSetCursorPosCallback(window_, cursorPosCallback);
    glfwSetScrollCallback(window_, scrollCallback);
    glfwSetFramebufferSizeCallback(window_, framebufferSizeCallback);

    return true;
}

// ============================================================
// HUD rendering
// ============================================================
void Renderer::renderButton(const Button& btn) {
    // Render a colored rectangle using the background shader trick
    // We'll just render it as a simple colored quad
    glDisable(GL_DEPTH_TEST);

    // Use the cube shader with identity matrices for 2D
    glUseProgram(cubeShader_);
    glm::mat4 identity(1.0f);
    // Map button coords (0-1) to clip space (-1 to 1)
    float x0 = btn.x * 2.0f - 1.0f;
    float y0 = (1.0f - btn.y - btn.h) * 2.0f - 1.0f;
    float x1 = (btn.x + btn.w) * 2.0f - 1.0f;
    float y1 = (1.0f - btn.y) * 2.0f - 1.0f;

    glm::vec3 color = btn.hovered ? btn.hoverColor : btn.color;

    std::vector<float> verts;
    glm::vec3 n(0, 0, 1);
    auto addV = [&](float x, float y) {
        verts.push_back(x); verts.push_back(y); verts.push_back(0);
        verts.push_back(n.x); verts.push_back(n.y); verts.push_back(n.z);
        verts.push_back(color.x); verts.push_back(color.y); verts.push_back(color.z);
    };
    addV(x0, y0); addV(x1, y0); addV(x1, y1);
    addV(x0, y0); addV(x1, y1); addV(x0, y1);

    glUniformMatrix4fv(glGetUniformLocation(cubeShader_, "model"), 1, GL_FALSE, glm::value_ptr(identity));
    glUniformMatrix4fv(glGetUniformLocation(cubeShader_, "view"), 1, GL_FALSE, glm::value_ptr(identity));
    glUniformMatrix4fv(glGetUniformLocation(cubeShader_, "projection"), 1, GL_FALSE, glm::value_ptr(identity));
    glUniform3f(glGetUniformLocation(cubeShader_, "lightPos"), 0, 0, 5);
    glUniform3f(glGetUniformLocation(cubeShader_, "viewPos"), 0, 0, 1);

    glBindVertexArray(cubeVAO_);
    glBindBuffer(GL_ARRAY_BUFFER, cubeVBO_);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(float), verts.data(), GL_DYNAMIC_DRAW);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
    glEnable(GL_DEPTH_TEST);
}

void Renderer::renderHUD(Cube& cube) {
    // Update button hover states
    float mx = (float)mouseX_ / width_;
    float my = (float)mouseY_ / height_;
    btnScramble_.hovered = btnScramble_.contains(mx, my);
    btnSolve_.hovered = btnSolve_.contains(mx, my);
    btnReset_.hovered = btnReset_.contains(mx, my);

    // Render buttons
    renderButton(btnScramble_);
    renderButton(btnSolve_);
    renderButton(btnReset_);

    // Render button labels
    float scale = 2.5f;
    float charW = 8.0f * scale;

    auto centerText = [&](const Button& btn, const std::string& text) {
        float textW = text.size() * charW;
        float bx = btn.x * width_;
        float bw = btn.w * width_;
        float by = (1.0f - btn.y - btn.h) * height_;
        float bh = btn.h * height_;
        float tx = bx + (bw - textW) / 2.0f;
        float ty = by + (bh - 8.0f * scale) / 2.0f;
        font_.renderText(text, tx, ty, scale, {1, 1, 1}, width_, height_);
    };

    centerText(btnScramble_, "SCRAMBLE");
    centerText(btnSolve_, "SOLVE");
    centerText(btnReset_, "RESET");

    // Status text (set by the main loop / input handlers)
    if (!statusText_.empty()) {
        glm::vec3 color = {0.9f, 0.9f, 0.95f};
        if (statusText_.find("SOLVED") != std::string::npos) {
            color = {0.2f, 1.0f, 0.3f};
        } else if (statusText_.find("UNSOLVABLE") != std::string::npos ||
                   statusText_.find("invalid") != std::string::npos) {
            color = {1.0f, 0.35f, 0.35f};
        }

        if (statusText_ == "SOLVED!") {
            font_.renderText(statusText_, width_ / 2.0f - 70, height_ - 55, 3.0f, color, width_, height_);
        } else {
            font_.renderText(statusText_, 10, height_ - 30, 1.8f, color, width_, height_);
        }
    }

    // Help text at bottom
    font_.renderText("RMB:Camera  LMB:Drag face  U/D/L/R/F/B:Moves  Shift:Reverse",
                     10, 5, 1.8f, {0.5f, 0.5f, 0.6f}, width_, height_);
}

bool Renderer::handleButtonClick(double x, double y, Cube& cube) {
    float mx = (float)x / width_;
    float my = (float)y / height_;

    if (btnScramble_.contains(mx, my)) {
        requestCancelSolve();
        cube.scramble(20);
        statusText_ = cube.isSolvable() ? "Scrambled" : "Scramble produced invalid state (reset)";
        if (!cube.isSolvable()) cube.reset();
        return true;
    }
    if (btnSolve_.contains(mx, my)) {
        if (solveFuture_.valid() &&
            solveFuture_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
            requestCancelSolve();
        } else {
            requestSolve(cube);
        }
        return true;
    }
    if (btnReset_.contains(mx, my)) {
        requestCancelSolve();
        cube.reset();
        while (!moveQueue_.empty()) moveQueue_.pop();
        currentAnim_.move = static_cast<Move>(255);
        statusText_ = "";
        return true;
    }
    return false;
}

void Renderer::run(Cube& cube, std::function<std::vector<Move>(Cube&, std::atomic_bool*, SolverProgress*)> solveFunc) {
    g_cube = &cube;
    solveFunc_ = std::move(solveFunc);

    float lastFrame = 0;
    while (!glfwWindowShouldClose(window_)) {
        float currentTime = (float)glfwGetTime();
        float dt = currentTime - lastFrame;
        lastFrame = currentTime;

        // Update levitation once per frame and keep it stable while dragging.
        if (!leftDragging_ && !rightDragging_) {
            levY_ = 0.15f * sinf(currentTime * 0.8f);
        }

        glfwPollEvents();

        // Solve progress / completion
        if (solveFuture_.valid()) {
            auto ready = (solveFuture_.wait_for(std::chrono::seconds(0)) == std::future_status::ready);
            if (ready) {
                auto solution = solveFuture_.get();

                bool cubeUnchanged = (cube.getState() == solveStartState_);
                if (!cubeUnchanged) {
                    statusText_ = "Cube changed; discarding solution";
                } else if (solveCancel_.load(std::memory_order_relaxed)) {
                    statusText_ = "Solve canceled";
                } else if (solution.empty()) {
                    statusText_ = cube.isSolved() ? "Solved" : "No solution found (depth limit)";
                } else {
                    for (auto m : solution) moveQueue_.push(m);
                    statusText_ = "Solution: " + std::to_string(solution.size()) + " moves";
                }
            } else {
                double elapsed = glfwGetTime() - solveStartTime_;
                uint64_t nodes = solveProgress_.nodes.load(std::memory_order_relaxed);
                int depth = solveProgress_.depth.load(std::memory_order_relaxed);
                if (depth < 0) {
                    statusText_ = "Building solver tables... " + std::to_string((int)elapsed) +
                                  "s (click SOLVE to cancel)";
                } else {
                    statusText_ = "Solving... depth " + std::to_string(depth) +
                                  "  nodes " + std::to_string((unsigned long long)nodes) +
                                  "  " + std::to_string((int)elapsed) + "s (click SOLVE to cancel)";
                }
            }
        } else if (cube.isSolved() && moveQueue_.empty() && currentAnim_.move == static_cast<Move>(255)) {
            statusText_ = "SOLVED!";
        }

        // Update animation
        if (currentAnim_.move != static_cast<Move>(255)) {
            currentAnim_.elapsed += dt;
            if (currentAnim_.elapsed >= currentAnim_.duration) {
                cube.applyMove(currentAnim_.move);
                currentAnim_.move = static_cast<Move>(255);
            }
        } else if (!moveQueue_.empty()) {
            currentAnim_.move = moveQueue_.front();
            moveQueue_.pop();
            currentAnim_.elapsed = 0.0f;
            currentAnim_.duration = 0.3f;
        }

        // Render
        glClearColor(0.1f, 0.1f, 0.15f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        renderBackground();
        renderCube(cube, currentTime);
        renderHUD(cube);

        glfwSwapBuffers(window_);
    }

    // Ensure the solve thread doesn't outlive `solveFunc_`/Renderer lifetime.
    requestCancelSolve();
    if (solveFuture_.valid()) {
        solveFuture_.wait();
        (void)solveFuture_.get();
    }
}

void Renderer::cleanup() {
    requestCancelSolve();
    if (solveFuture_.valid()) {
        solveFuture_.wait();
        (void)solveFuture_.get();
    }
    font_.cleanup();
    glDeleteVertexArrays(1, &cubeVAO_);
    glDeleteBuffers(1, &cubeVBO_);
    glDeleteVertexArrays(1, &bgVAO_);
    glDeleteBuffers(1, &bgVBO_);
    glDeleteProgram(cubeShader_);
    glDeleteProgram(bgShader_);
    glfwDestroyWindow(window_);
    glfwTerminate();
}

void Renderer::requestSolve(Cube& cube) {
    if (!solveFunc_) {
        statusText_ = "Solver not available";
        return;
    }
    if (!cube.isSolvable()) {
        statusText_ = "Cube is UNSOLVABLE (reset)";
        return;
    }
    if (currentAnim_.move != static_cast<Move>(255) || !moveQueue_.empty()) {
        statusText_ = "Wait for moves to finish";
        return;
    }
    if (solveFuture_.valid() &&
        solveFuture_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
        statusText_ = "Already solving (click SOLVE to cancel)";
        return;
    }

    solveCancel_.store(false, std::memory_order_relaxed);
    solveProgress_.nodes.store(0, std::memory_order_relaxed);
    solveProgress_.depth.store(0, std::memory_order_relaxed);
    solveStartState_ = cube.getState();
    solveStartTime_ = glfwGetTime();
    statusText_ = "Solving...";

    Cube work = cube;
    solveFuture_ = std::async(std::launch::async, [this, work]() mutable {
        return solveFunc_(work, &solveCancel_, &solveProgress_);
    });
}

void Renderer::requestCancelSolve() {
    if (solveFuture_.valid() &&
        solveFuture_.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
        solveCancel_.store(true, std::memory_order_relaxed);
        statusText_ = "Canceling solve...";
    }
}
