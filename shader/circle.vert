#version 460

// could use the radius here directly
vec2 offsets[4] = {
	vec2(-1.0, -1.0), vec2(1.0, -1.0),
	vec2(-1.0, 1.0), vec2(1.0, 1.0),
};

int indices[6] = {
	0, 1, 2,
	2, 1, 3,
};

uniform sampler1D positions;
uniform float radius;

out vec2 uv;

struct Particle {
	vec2 pos;
	vec2 prevPos;
};

layout(std430, binding = 0) buffer Particles {
	Particle particles[];
};

void main(void) {
	int ii = gl_InstanceID;
	int vi = gl_VertexID;
	// vec2 position = texelFetch(positions, ii, 0).xy;
	vec2 position = particles[ii].pos;
	vec2 offset = offsets[indices[vi]];
	uv = offset;
	gl_Position = vec4(position + radius * offset, 0.0, 1.0);
}
