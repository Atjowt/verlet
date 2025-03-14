#version 460

in flat int InstanceID;
in vec2 TexCoord;
out vec4 FragColor;

vec3 palette(int i) {
	return vec3(0.2) + vec3((i % 7) / 7.0, (i % 11) / 11.0, (i % 13) / 13.0);
}

void main(void) {
	if (length(TexCoord) > 1.0) discard;
	// FragColor = vec4(TexCoord, 0.0, 1.0);
	// FragColor = vec4(0.2, 0.4, 1.0, 1.0);
	FragColor = vec4(palette(InstanceID), 1.0);
}
