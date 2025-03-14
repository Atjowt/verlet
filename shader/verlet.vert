#version 460

float radius = 0.04;
float bounce = 0.8f;
float friction = 0.8f;

vec2 quad[4] = {
	vec2(-1.0, -1.0), vec2(1.0, -1.0),
	vec2(-1.0, 1.0), vec2(1.0, 1.0),
};

int indices[6] = {
	0, 1, 2,
	2, 1, 3,
};

uniform float uDeltaTime;
uniform vec4 uMouse;

layout(std430, binding = 0) buffer PosBuf {
	vec2 pos[];
};

layout(std430, binding = 1) buffer VelBuf {
	vec2 vel[];
};

layout(std430, binding = 2) buffer AccBuf {
	vec2 acc[];
};

out flat int InstanceID;
out vec2 TexCoord;

vec2 applyForces(vec2 p) {
	vec2 a = vec2(0.0, 0.0);
	a += vec2(0.0, -8.0); // gravity
	a += 32.0 * (uMouse.xy - p) * uMouse.z; // attract LMB
	a -= 32.0 * (uMouse.xy - p) * uMouse.w; // repulse RMB
	return a;
}

void main(void) {

	float dt = uDeltaTime;

	vec2 p = pos[gl_InstanceID];
	vec2 v = vel[gl_InstanceID];
	vec2 a = acc[gl_InstanceID];

	if (p.x < -1.0+radius) {
		p.x = -1.0+radius;
		v.x = -bounce*v.x;
		v.y = friction*v.y;
	}

	if (p.x > +1.0-radius) {
		p.x = +1.0-radius;
		v.x = -bounce*v.x;
		v.y = friction*v.y;
	}

	if (p.y < -1.0+radius) {
		p.y = -1.0+radius;
		v.y = -bounce*v.y;
		v.x = friction*v.x;
	}

	if (p.y > +1.0-radius) {
		p.y = +1.0-radius;
		v.y = -bounce*v.y;
		v.x = friction*v.x;
	}

	vec2 offset = quad[indices[gl_VertexID]];
	TexCoord = offset;
	InstanceID = gl_InstanceID;
	gl_Position = vec4(p + radius * offset, 0.0, 1.0);

	vec2 np = p + v*dt + 0.5*a*dt*dt;
	vec2 na = applyForces(np);
	vec2 nv = v + 0.5*(a+na)*dt;

	pos[gl_InstanceID] = np;
	vel[gl_InstanceID] = nv;
	acc[gl_InstanceID] = na;
}
