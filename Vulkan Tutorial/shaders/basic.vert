#version 450
#extension GL_ARB_separate_shader_objects : enable

layout (binding = 0) uniform UniformBufferObject {
    mat4 model;
    mat4 view;
    mat4 position;
} ubo;

layout (location = 0) in vec3 a_Pos;
layout (location = 1) in vec3 a_Color;
layout (location = 2) in vec2 a_TexCoords;

layout(location = 0) out vec3 v_FragColor;
layout(location = 1) out vec2 v_TexCoords;
layout(location = 2) out vec4 v_TintColor;

const vec3 DELTA = vec3(0.5);



layout (push_constant) uniform color {
    vec4 pc_TintColor;
} tc;

void main() {
    vec3 vectorDelta = gl_InstanceIndex * DELTA;
    vec3 position = a_Pos + vectorDelta;
    gl_Position = ubo.position  * ubo.view * ubo.model * vec4(position, 1.0);
    v_FragColor = a_Color;
    v_TexCoords = a_TexCoords;
    v_TintColor = tc.pc_TintColor;
}