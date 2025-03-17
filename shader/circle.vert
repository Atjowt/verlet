const vec2 offsets[4] = {
	vec2(-1.0, -1.0), vec2(1.0, -1.0),
	vec2(-1.0, 1.0), vec2(1.0, 1.0),
};

const int indices[6] = {
	0, 1, 2,
	2, 1, 3,
};

out vec2 uv;

void main(void) {

	int ii = gl_InstanceID;
	int vi = gl_VertexID;

	vec2 position = Particles[ii].xy;
	vec2 offset = offsets[indices[vi]];

	uv = offset;
	gl_Position = vec4(position + RADIUS * offset, 0.0, 1.0);
}
