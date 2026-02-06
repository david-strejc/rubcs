#pragma once
#include <GL/glew.h>
#include <string>
#include <glm/glm.hpp>

// Minimal bitmap font renderer using embedded 8x8 font
class Font {
public:
    void init();
    void cleanup();
    void renderText(const std::string& text, float x, float y, float scale,
                    const glm::vec3& color, int screenW, int screenH);

private:
    GLuint shader_ = 0;
    GLuint vao_ = 0, vbo_ = 0;
    GLuint texture_ = 0;
};

// Simple button
struct Button {
    float x, y, w, h; // normalized coords (0-1 of screen)
    std::string label;
    glm::vec3 color;
    glm::vec3 hoverColor;
    bool hovered = false;

    bool contains(float mx, float my) const {
        return mx >= x && mx <= x + w && my >= y && my <= y + h;
    }
};
