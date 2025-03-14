#version 460

float radius = 0.01;
float bounce = 0.5f;
float friction = 0.5f;

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
	a += 32.0 * (uMouse.xy - p) * uMouse.z; // attract
	a -= 32.0 * (uMouse.xy - p) * uMouse.w; // repulse
	return a;
}

void collide(int i1, int i2) {

	vec2 p1 = pos[i1];
	vec2 p2 = pos[i2];

	vec2 v1 = vel[i1];
	vec2 v2 = vel[i2];

	vec2 delta = p1 - p2;
	float sqrdist = dot(delta, delta);

	float mindist = 2.0 * radius;

	float epsilon = 0.0001;
	if (sqrdist >= mindist*mindist || sqrdist < epsilon*epsilon) {
		return;
	}

	float dist = sqrt(sqrdist);

	vec2 normal = delta / dist;
	vec2 tangent = vec2(-normal.y, normal.x);

	float v1n = dot(v1, normal);
	float v1t = dot(v1, tangent);

	float v2n = dot(v2, normal);
	float v2t = dot(v2, tangent);

	float v1nNew = v2n;
	float v2nNew = v1n;

	vec2 v1New = v1nNew * normal + v1t * tangent;
	vec2 v2New = v2nNew * normal + v2t * tangent;

	// float loss = 0.995;
	float loss = 1.0;
	vel[i1] = loss * v1New;
	vel[i2] = loss * v2New;

	float overlap = mindist - dist;
	float sep = 0.5;
	pos[i1] += sep * overlap * normal;
	pos[i2] -= sep * overlap * normal;

}

void main(void) {

	float dt = uDeltaTime;
	// float dt = 0.001;

	int n = pos.length();
	for (int step = 0; step < 1; step++) {
		for (int i = 0; i < gl_InstanceID; i++) {
			collide(gl_InstanceID, i);
		}
		for (int i = gl_InstanceID + 1; i < n; i++) {
			collide(gl_InstanceID, i);
		}
	}

	vec2 p = pos[gl_InstanceID];
	vec2 v = vel[gl_InstanceID];
	vec2 a = acc[gl_InstanceID];

	if (p.x < -1.0 + radius) {
		p.x = -1.0 + radius;
		v.x = -bounce * v.x;
		v.y = friction * v.y;
	}

	if (p.x > 1.0 - radius) {
		p.x = 1.0 - radius;
		v.x = -bounce * v.x;
		v.y = friction * v.y;
	}

	if (p.y < -1.0 + radius) {
		p.y = -1.0 + radius;
		v.y = -bounce * v.y;
		v.x = friction * v.x;
	}

	if (p.y > 1.0 - radius) {
		p.y = 1.0 - radius;
		v.y = -bounce * v.y;
		v.x = friction * v.x;
	}

	vec2 offset = quad[indices[gl_VertexID]];
	TexCoord = offset;
	InstanceID = gl_InstanceID;
	gl_Position = vec4(p + radius * offset, 0.0, 1.0);

	vec2 np = p + v * dt + 0.5 * a * dt * dt;
	vec2 na = applyForces(np);
	vec2 nv = v + 0.5 * (a + na) * dt;

	pos[gl_InstanceID] = np;
	vel[gl_InstanceID] = nv;
	acc[gl_InstanceID] = na;
}
