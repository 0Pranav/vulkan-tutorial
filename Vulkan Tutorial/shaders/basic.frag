#version 450
#extension GL_ARB_separate_shader_objects : enable


layout(location = 0) in vec3 v_FragNormal;
layout(location = 1) in vec2 v_TexCoords;
layout(location = 2) in vec3 v_FragPos;

layout (binding = 1) uniform sampler2D texSampler;
layout (binding = 4) uniform sampler2D specSampler;
layout (binding = 2) uniform Light {
    vec3 position;
    vec3 color;
} light;

layout (binding = 3) uniform Camera {
    vec3 position;
} camera;

layout(location = 0) out vec4 outColor;

void main() {
    // ambient lighting
    float ambientStrength = 0.05;
    vec3 ambient = ambientStrength * light.color;
    vec3 objectColor = texture(texSampler, v_TexCoords).xyz;

    // diffuse lighting
    vec3 norm = normalize(v_FragNormal);
    vec3 lightDir = normalize(light.position - v_FragPos);  
    float diff = max(dot(norm, lightDir), 0.0);
    vec3 diffuse = diff * light.color;

    // specular
    vec3 viewDir = normalize(camera.position - v_FragPos);
    vec3 reflectDir = reflect(-lightDir, norm);  
    float spec = pow(max(dot(viewDir, reflectDir), 0.0), 32);
    vec3 specular = vec3(texture(specSampler, v_TexCoords)) * spec * light.color ;  

    // combine
    vec3 result = (/*ambient + diffuse*/ + specular) * objectColor;
    outColor = vec4(result, 1.0);
}