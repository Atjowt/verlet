#version 460

#define RADIUS 0.1
#define MOUSE_FORCE 16.0

#define CELL_SIZE (2.0 * RADIUS)
#define GRID_SIZE 10
#define CELL_CAPACITY 8

layout(std430, binding = 0) buffer ParticleBuffer {
	vec4 Particles[];
};

layout(std430, binding = 1) buffer IndexBuffer {
	uint Indices[];
};

layout(std430, binding = 2) buffer CountBuffer {
	uint Counts[];
};

