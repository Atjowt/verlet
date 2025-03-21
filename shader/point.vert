#version 460

layout(location = 0) in vec2 position;

out vec3 VertColor;

vec3 palette(int i) {
	return vec3(0.0) + vec3((i % 7) / 7.0, (i % 11) / 11.0, (i % 13) / 13.0);

}
void main(void) {
	gl_Position = vec4(position, 0.0, 1.0);
	VertColor = palette(gl_VertexID);
	// VertColor = vec3(0.0, 0.0, 1.0);
}
