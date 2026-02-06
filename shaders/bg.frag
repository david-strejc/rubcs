#version 330 core

in vec2 TexCoord;
out vec4 FragColor;

void main() {
    // Gradient from dark blue-purple (top) to dark navy (bottom)
    vec3 topColor = vec3(0.102, 0.102, 0.180);   // #1a1a2e
    vec3 botColor = vec3(0.086, 0.129, 0.243);    // #16213e
    vec3 color = mix(botColor, topColor, TexCoord.y);
    FragColor = vec4(color, 1.0);
}
