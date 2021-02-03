#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding = 0) uniform MVPMatrices {
	mat4 model;
	mat4 view;
	mat4 proj;
} mvp;

layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;
layout(location = 2) in vec2 uv;

layout(location = 0) out vec3 fragColor;
layout(location = 1) out vec2 outUV;

void main() {
	gl_Position = mvp.proj * mvp.view * mvp.model * vec4(position, 1.0);
	fragColor = color;
	outUV = uv;
}

