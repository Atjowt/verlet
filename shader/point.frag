#version 460

in vec3 VertColor;
out vec4 FragColor;

void main(void) {
	vec2 p = gl_PointCoord * 2.0 - 1.0;
	float d2 = dot(p, p);
	vec2 q = vec2(0.7, 0.7);
	float l = 1.0 - distance(p, vec2(-0.5, -0.5));
	float s = 0.5 * -dot(p+0.4, q+0.4) + 0.5;
	if (d2 > 1.0) discard;
	vec3 col = VertColor;
	col = mix(0.3*col, col, 1.0-l);
	col = mix(col, vec3(1.0), l*l);
	FragColor = vec4(col, 1.0);
}
