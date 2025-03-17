#version 460

const vec2 offsets[4] = {
	vec2(-1.0, -1.0), vec2(1.0, -1.0),
	vec2(-1.0, 1.0), vec2(1.0, 1.0),
};

const int indices[6] = {
	0, 1, 2,
	2, 1, 3,
};

uniform float radius;

out vec2 uv;

layout (std430, binding=0) buffer PositionBuffer {
	vec2 positions[];
};

void main(void) {

	int ii = gl_InstanceID;
	int vi = gl_VertexID;

	vec2 position = positions[ii].xy;
	vec2 offset = offsets[indices[vi]];

	uv = offset;
	gl_Position = vec4(position + radius * offset, 0.0, 1.0);
}
