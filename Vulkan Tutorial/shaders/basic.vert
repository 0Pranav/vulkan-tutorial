#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 position;
} ubo;

layout (location = 0) in vec3 a_Pos;
layout (location = 1) in vec3 a_Normal;
layout (location = 2) in vec2 a_TexCoords;

layout(location = 0) out vec3 v_FragNormal;
layout(location = 1) out vec2 v_TexCoords;
layout(location = 2) out vec3 v_FragPos;


void main() {
    gl_Position  = ubo.position  * ubo.view * ubo.model * vec4(a_Pos, 1.0);
    v_FragNormal = mat3(transpose(inverse(ubo.model)))  * a_Normal;
    v_TexCoords  = a_TexCoords;
}