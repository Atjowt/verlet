#version 460

layout(location = 0) in vec2 position;

uniform float radius;

out flat int vid;

void main(void) {
	gl_Position = vec4(position, 0.0, 1.0);
	vid = gl_VertexID;
}
