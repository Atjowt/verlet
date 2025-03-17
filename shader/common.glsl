#version 460

#define RADIUS 0.1
#define MOUSE_FORCE 16.0

layout(std430, binding = 0) buffer ParticleBuffer {
	vec4 Particles[];
};
