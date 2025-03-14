#version 460

layout(std430, binding = 0) buffer PosBuf {
	vec2 pos[];
};

layout(std430, binding = 1) buffer VelBuf {
	vec2 vel[];
};

layout(std430, binding = 2) buffer AccBuf {
	vec3 acc[];
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

	pos[gl_InstanceID] += vec2(100.0, 100.0) * dt;

	gl_Position = vec4(p + s * v, 0.0, 1.0);
}
