#version 460

in vec3 VertexColor;
in vec2 TexCoord;

out vec4 FragColor;

void main(void) {
	// FragColor = vec4(TexCoord, 0.0, 1.0);
	FragColor = vec4(VertexColor, 1.0);
}
