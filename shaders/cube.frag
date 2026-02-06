#version 330 core

in vec3 FragPos;
in vec3 Normal;
in vec3 ObjColor;

uniform vec3 lightPos;
uniform vec3 viewPos;

out vec4 FragColor;

void main() {
    // Ambient
    float ambientStrength = 0.18;
    vec3 ambient = ambientStrength * ObjColor;

    // Diffuse
    vec3 norm = normalize(Normal);
    vec3 lightDir = normalize(lightPos - FragPos);
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * ObjColor;

    // Specular (Phong)
    float specularStrength = 0.4;
    vec3 viewDir = normalize(viewPos - FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32.0);
    vec3 specular = specularStrength * spec * vec3(1.0);

    // Second fill light from below-left for softer shadows
    vec3 lightDir2 = normalize(vec3(-4.0, -3.0, 2.0) - FragPos);
    float diff2 = max(dot(norm, lightDir2), 0.0);
    vec3 fill = 0.12 * diff2 * ObjColor;

    vec3 result = ambient + diffuse + specular + fill;
    FragColor = vec4(result, 1.0);
}
