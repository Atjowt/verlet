#version 460

in flat int vid;
out vec4 FragColor;

vec3 palette(int i) {
	return vec3(0.2) + vec3((i % 7) / 7.0, (i % 11) / 11.0, (i % 13) / 13.0);
}

void main(void) {
	vec2 p = gl_PointCoord * 2.0 - 1.0;
	float d2 = dot(p, p);
	if (d2 > 1.0) discard;
	FragColor = vec4(palette(vid), 1.0);
}
