#version 450

layout(location = 0) in vec3 in_position;
layout(location = 1) in vec3 in_color;

layout(set = 0, binding = 0) uniform Uniforms {
	mat4 model;
	mat4 view;
	mat4 projection;
	vec4 color;
} u;

layout(location = 0) out vec3 out_color;
layout(location = 1) out vec3 out_normal;

void main() {
	gl_Position = u.projection * u.view * u.model * vec4(in_position, 1.0);
	out_color = u.color.rgb * in_color;
	out_normal = transpose(inverse(mat3(u.model))) * in_position;
}
