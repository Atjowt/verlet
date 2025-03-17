#version 460

#define RADIUS 0.02

// #define CELL_SIZE (2.0 * RADIUS)
// #define GRID_SIZE 10
// #define CELL_CAPACITY 8

layout(std430, binding = 0) buffer ParticleBuffer {
	vec4 Particles[];
};

layout(std430, binding = 1) buffer SeparationBuffer {
	vec2 Separations[];
};

