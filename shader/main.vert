#version 460

int index[6] = {
	0, 1, 2,
	2, 1, 3,
};

vec2 vertex[4] = {
	vec2(-1.0, -1.0), vec2(1.0, -1.0),
	vec2(-1.0, 1.0), vec2(1.0, 1.0),
};

#define INSTANCES 4

vec2 pos[INSTANCES] = {
	vec2(0.0, 0.0),
	vec2(0.1, 0.1),
	vec2(0.2, 0.2),
	vec2(0.3, 0.3),
};

vec2 scale[INSTANCES] = {
	vec2(0.1, 0.1),
	vec2(0.1, 0.1),
	vec2(0.1, 0.1),
	vec2(0.1, 0.1),
};

vec3 color[INSTANCES] = {
	vec3(1.0, 0.0, 0.0),
	vec3(0.0, 1.0, 0.0),
	vec3(0.0, 0.0, 1.0),
	vec3(1.0, 1.0, 0.0),
};

out vec3 VertexColor;
out vec2 TexCoord;

uniform float uDeltaTime;

void main(void) {
	float dt = uDeltaTime;
	vec2 v = vertex[index[gl_VertexID]];
	vec2 p = pos[gl_InstanceID];
	vec2 s = scale[gl_InstanceID];
	VertexColor = color[gl_InstanceID];
	TexCoord = (v + 1.0) * 0.5;
	pos[gl_InstanceID] += vec2(100.0, 100.0) * dt; // this does not work
	gl_Position = vec4(p + s * v, 0.0, 1.0);
}
