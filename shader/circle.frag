#version 460

in vec2 uv;
out vec4 color;

// vec3 palette(int i) {
// 	return vec3(0.2) + vec3((i % 7) / 7.0, (i % 11) / 11.0, (i % 13) / 13.0);
// }

void main(void) {
	float len2 = dot(uv, uv);
	if (len2 > 1.0) discard;
	color = vec4(uv, 1.0, 1.0);
}
