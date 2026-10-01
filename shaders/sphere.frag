#version 450

layout(location = 0) in vec3 in_color;
layout(location = 1) in vec3 in_normal;

layout(location = 0) out vec4 out_color;

void main() {
	vec3 light = normalize(vec3(1.0, 2.0, 1.5));
	float diffuse = max(dot(normalize(in_normal), light), 0.0);
	out_color = vec4(in_color * (0.3 + 0.7 * diffuse), 1.0);
}
