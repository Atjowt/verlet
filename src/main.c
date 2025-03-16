#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#define RANDOM() (rand() / (float)RAND_MAX)
#define INITIAL_PARTICLES 512
#define PARTICLE_RADIUS 0.01f
#define GRAVITY 4.0f
#define MOUSE_FORCE 32.0f
#define TIMESTEP 0.0008f
#define DAMPENING 1.0f
#define RADIUS_INNER 0.2f
#define RADIUS_OUTER (1.0f - PARTICLE_RADIUS)
#define CELL_SIZE (2.0f * PARTICLE_RADIUS)
#define GRID_WIDTH (int)(2.0f / CELL_SIZE + 0.999f)
#define GRID_HEIGHT (int)(2.0f / CELL_SIZE + 0.999f)
#define CELL_CAPACITY 16
#define MAX_INFO_LOG 512

bool compileShader(const GLchar shaderSource[], GLsizei sourceLength, GLuint* shader) {
	glShaderSource(*shader, 1, (const GLchar* []) { shaderSource }, &sourceLength);
	glCompileShader(*shader);
	GLint shaderCompiled;
	glGetShaderiv(*shader, GL_COMPILE_STATUS, &shaderCompiled);
	if (shaderCompiled != GL_TRUE) {
		GLchar infoLog[MAX_INFO_LOG];
		GLsizei logLength;
		glGetShaderInfoLog(*shader, sizeof(infoLog), &logLength, infoLog);
		fprintf(stderr, "Failed to compile shader:\n%.*s\n", logLength, infoLog);
		return false;
	}
	return true;
}

bool linkProgram(GLuint* shaderProgram) {
	glLinkProgram(*shaderProgram);
	GLint programLinked;
	glGetProgramiv(*shaderProgram, GL_LINK_STATUS, &programLinked);
	if (programLinked != GL_TRUE) {
		GLchar infoLog[MAX_INFO_LOG];
		GLsizei logLength;
		glGetProgramInfoLog(*shaderProgram, sizeof(infoLog), &logLength, infoLog);
		fprintf(stderr, "Failed to link shader program:\n%.*s\n", logLength, infoLog);
		return false;
	}
	return true;
}

void glfwErrorCallback(int code, const char* desc);
void glfwCursorPosCallback(GLFWwindow* window, double x, double y);
void glfwMouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
void glfwFramebufferSizeCallback(GLFWwindow* window, int width, int height);

float screen[2] = { 0.0f, 0.0f };
float mouse[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

struct {
	int count;
	float* x;
	float* y;
	float* px;
	float* py;
} particles;

struct {
	int** indices;
	int* counts;
} grid;

void initGrid(void) {
	grid.counts = malloc(GRID_WIDTH * GRID_HEIGHT * sizeof(int));
	grid.indices = malloc(GRID_WIDTH * GRID_HEIGHT * sizeof(int*));
	for (int ci = 0; ci < GRID_WIDTH * GRID_HEIGHT; ci++) {
		grid.indices[ci] = malloc(CELL_CAPACITY * sizeof(int));
	}
}

void freeGrid(void) {
	for (int ci = 0; ci < GRID_WIDTH * GRID_HEIGHT; ci++) {
		free(grid.indices[ci]);
	}
	free(grid.indices);
	free(grid.counts);
}

// int gridPopParticle(int i, int ci) {
// 	int n = grid.counts[ci];
// 	int swapback = grid.indices[ci][n-1];
// 	int pi = grid.indices[ci][i];
// 	grid.indices[ci][i] = swapback;
// 	--grid.counts[ci];
// 	return pi;
// }

void recalculateGrid(void) {
	memset(grid.counts, 0, GRID_WIDTH * GRID_HEIGHT * sizeof(int));
	for (int pi = 0; pi < particles.count; pi++) {
		float x = particles.x[pi];
		float y = particles.y[pi];
		int cx = (x + 1.0f) / CELL_SIZE;
		if (cx < 0) { cx = 0; }
		if (cx >= GRID_WIDTH) { cx = GRID_WIDTH - 1; }
		int cy = (y + 1.0f) / CELL_SIZE;
		if (cy < 0) { cy = 0; }
		if (cy >= GRID_HEIGHT) { cy = GRID_HEIGHT - 1; }
		int ci = cy * GRID_WIDTH + cx;
		int i = grid.counts[ci];
		// if (i < 0) {
		// 	fprintf(stderr, "Index negative for some reason! (%d)\n", i);
		// 	continue;
		// }
		if (i >= CELL_CAPACITY) {
			fprintf(stderr, "Too many particles in cell! (%d)\n", i);
			continue;
		}
		grid.indices[ci][i] = pi;
		grid.counts[ci]++;
	}
}

void initParticles(size_t count) {
	particles.count = count;
	particles.x = malloc(count * sizeof(*particles.x));
	particles.y = malloc(count * sizeof(*particles.y));
	particles.px = malloc(count * sizeof(*particles.px));
	particles.py = malloc(count * sizeof(*particles.py));
	for (int i = 0; i < count; i++) {
		particles.x[i] = (RANDOM() - 0.5f) * 2.0f;
		particles.y[i] = (RANDOM() - 0.5f) * 2.0f;
		// particles.px[i] = particles.x[i] - (RANDOM() - 0.5f) * 0.001f;
		// particles.py[i] = particles.y[i] - (RANDOM() - 0.5f) * 0.001f;
		particles.px[i] = particles.x[i];
		particles.py[i] = particles.y[i];
	}
}

void freeParticles(void) {
	free(particles.x);
	free(particles.y);
	free(particles.px);
	free(particles.py);
	particles.count = 0;
}

void moveParticleX(int i, float dt) {
	float x = particles.x[i];
	float ax = 0.0f;
	ax += mouse[2] * 4.0f * (mouse[0] - x);
	float px = particles.px[i];
	float dx = x - px;
	particles.px[i] = x;
	particles.x[i] = x + DAMPENING * dx + ax * dt * dt;
	// particles.x[i] = x + dx + ax * dt * dt;
}

void moveParticleY(int i, float dt) {
	float y = particles.y[i];
	float ay = 0.0f;
	ay -= GRAVITY;
	ay += mouse[2] * MOUSE_FORCE * (mouse[1] - y);
	float py = particles.py[i];
	float dy = y - py;
	particles.py[i] = y;
	// particles.y[i] = y + dy + ay * dt * dt;
	particles.y[i] = y + DAMPENING * dy + ay * dt * dt;
}

void moveParticle(int i, float dt) {
	moveParticleX(i, dt);
	moveParticleY(i, dt);
}

void moveParticles(float dt) {
	for (int i = 0; i < particles.count; i++) {
		moveParticle(i, dt);
	}
}

void constrainParticle(int i) {
	float x = particles.x[i];
	float y = particles.y[i];
	float  dist2 = x * x + y * y;
	float dist = sqrtf(dist2);
	float invdist = 1.0f / dist;
	float nx = x * invdist;
	float ny = y * invdist;
	dist = fminf(dist, RADIUS_OUTER);
	dist = fmaxf(dist, RADIUS_INNER);
	particles.x[i] = nx * dist;
	particles.y[i] = ny * dist;
}

void constrainParticles(void) {
	for (int i = 0; i < particles.count; i++) {
		constrainParticle(i);
	}
}

void collideParticle(int i1, int i2) {
	float x1 = particles.x[i1];
	float y1 = particles.y[i1];
	float x2 = particles.x[i2];
	float y2 = particles.y[i2];
	float dx = x1 - x2;
	float dy = y1 - y2;
	float dist2 = dx * dx + dy * dy;
	float rsum = 2.0f * PARTICLE_RADIUS;
	float rsum2 = rsum * rsum;
	if (dist2 >= rsum2) {
		return;
	}
	float dist = sqrtf(dist2);
	float epsilon = 0.0000001f;
	float nx, ny;
	if (dist < epsilon) {
		nx = 0.0f;
		ny = 1.0f;
	} else {
		float invdist = 1.0f / dist;
		nx = dx * invdist;
		ny = dy * invdist;
	}
	float overlap = rsum - dist;
	float scalar = 1.05f; // adjust for overhoot/undershoot in separation
	particles.x[i1] += scalar * 0.5f * overlap * nx;
	particles.y[i1] += scalar * 0.5f * overlap * ny;
	particles.x[i2] -= scalar * 0.5f * overlap * nx;
	particles.y[i2] -= scalar * 0.5f * overlap * ny;
}

void collideParticlesNaive(void) {
	for (int i1 = 0; i1 < particles.count - 1; i1++) {
		for (int i2 = i1; i2 < particles.count; i2++) {
			collideParticle(i1, i2);
		}
	}
}

void shuffleParticles(void) {
	for (int i = particles.count - 1; i > 0; i--) {
		int j = rand() % (i + 1);
		float temp = particles.x[i];
		particles.x[i] = particles.x[j];
		particles.x[j] = temp;
		temp = particles.y[i];
		particles.y[i] = particles.y[j];
		particles.y[j] = temp;
		temp = particles.px[i];
		particles.px[i] = particles.px[j];
		particles.px[j] = temp;
		temp = particles.py[i];
		particles.py[i] = particles.py[j];
		particles.py[j] = temp;
	}
}

void collideParticlesGrid(void) {
	// shuffleParticles();
	recalculateGrid();
	#pragma omp parallel for schedule(dynamic)
	for (int cy = 1; cy < GRID_HEIGHT - 1; cy++) {
		for (int cx = 1; cx < GRID_WIDTH - 1; cx++) {
			int indices[9 * CELL_CAPACITY];
			int count = 0;
			for (int dy = -1; dy <= 1; dy++) {
				for (int dx = -1; dx <= 1; dx++) {
					int ci = (cy + dy) * GRID_WIDTH + cx + dx;
					for (int i = 0; i < grid.counts[ci]; i++) {
						indices[count++] = grid.indices[ci][i];
					}
				}
			}
			for (int i = 0; i < count - 1; i++) {
				for (int j = i + 1; j < count; j++) {
					collideParticle(indices[i], indices[j]);
				}
			}
		}
	}
}

void collideParticles(void) {
	collideParticlesGrid();
	// collideParticlesNaive();
}

void updateParticles(float dt) {
	moveParticles(dt);
	collideParticles();
	constrainParticles();
}

int main(void) {

	int exitcode = 0;

	glfwInitHint(GLFW_WAYLAND_LIBDECOR, GLFW_WAYLAND_DISABLE_LIBDECOR);

	if (!glfwInit()) {
		fprintf(stderr, "Failed to initialize GLFW\n");
		exitcode = 1;
		goto terminate;
	}

	printf("GLFW %s\n", glfwGetVersionString());

	glfwSetErrorCallback(glfwErrorCallback);

	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	GLFWwindow* window = glfwCreateWindow(1024, 1024, "Verlet Integration", NULL, NULL);
	if (!window) {
		fprintf(stderr, "Failed to create GLFW window\n");
		exitcode = 1;
		goto terminate;
	}

	glfwSetFramebufferSizeCallback(window, glfwFramebufferSizeCallback);
	glfwSetCursorPosCallback(window, glfwCursorPosCallback);
	glfwSetMouseButtonCallback(window, glfwMouseButtonCallback);

	glfwMakeContextCurrent(window);

	glfwSwapInterval(0); // VSync

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
		fprintf(stderr, "Failed to initialize GLAD\n");
		exitcode = 1;
		goto terminate;
	}

	printf("OpenGL %s\n", glGetString(GL_VERSION));

	printf("Compiling shader 'circle.vert'...\n");
	GLuint circleVertShader = glCreateShader(GL_VERTEX_SHADER); {
		const GLchar shaderSource[] = {
			#embed "shader/circle.vert"
		};
		GLsizei sourceLength = sizeof(shaderSource);
		compileShader(shaderSource, sourceLength, &circleVertShader);
	}

	printf("Compiling shader 'circle.frag'...\n");
	GLuint circleFragShader = glCreateShader(GL_FRAGMENT_SHADER); {
		const GLchar shaderSource[] = {
			#embed "shader/circle.frag"
		};
		GLsizei sourceLength = sizeof(shaderSource);
		compileShader(shaderSource, sourceLength, &circleFragShader);
	}

	printf("Linking program 'circleShader'...\n");
	GLuint circleShader = glCreateProgram();
	glAttachShader(circleShader, circleVertShader);
	glAttachShader(circleShader, circleFragShader);
	linkProgram(&circleShader);

	srand(time(NULL));

	initParticles(INITIAL_PARTICLES);
	initGrid();

	GLuint positionTex;
	glGenTextures(1, &positionTex);
	glBindTexture(GL_TEXTURE_1D, positionTex);
	glTexImage1D(GL_TEXTURE_1D, 0, GL_RG32F, particles.count, 0, GL_RG, GL_FLOAT, NULL);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_1D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glBindTexture(GL_TEXTURE_1D, 0);

	GLuint particlesSSBO;
	glGenBuffers(1, &particlesSSBO);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, particlesSSBO);
	glBufferData(GL_SHADER_STORAGE_BUFFER, particles.count * 4 * sizeof(float), NULL, GL_DYNAMIC_DRAW);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, particlesSSBO);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	GLuint adjustmentsSSBO;
	glGenBuffers(1, &adjustmentsSSBO);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, adjustmentsSSBO);
	glBufferData(GL_SHADER_STORAGE_BUFFER, particles.count * 2 * sizeof(float), NULL, GL_DYNAMIC_DRAW);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, adjustmentsSSBO);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	printf("Compiling shader 'move.comp'...\n");
	GLuint movementComputeShader = glCreateShader(GL_COMPUTE_SHADER); {
		const GLchar shaderSource[] = {
			#embed "shader/move.comp"
		};
		GLsizei sourceLength = sizeof(shaderSource);
		compileShader(shaderSource, sourceLength, &movementComputeShader);
	}

	printf("Linking program 'moveComputeProgram'...\n");
	GLuint movementComputeProgram = glCreateProgram();
	glAttachShader(movementComputeProgram, movementComputeShader);
	linkProgram(&movementComputeProgram);

	printf("Compiling shader 'collide.comp'...\n");
	GLuint collisionComputeShader = glCreateShader(GL_COMPUTE_SHADER); {
		const GLchar shaderSource[] = {
			#embed "shader/collide.comp"
		};
		GLsizei sourceLength = sizeof(shaderSource);
		compileShader(shaderSource, sourceLength, &collisionComputeShader);
	}

	printf("Linking program 'collideComputeProgram'...\n");
	GLuint collisionComputeProgram = glCreateProgram();
	glAttachShader(collisionComputeProgram, collisionComputeShader);
	linkProgram(&collisionComputeProgram);
	
	printf("Compiling shader 'constrain.comp'...\n");
	GLuint constrainComputeShader = glCreateShader(GL_COMPUTE_SHADER); {
		const GLchar shaderSource[] = {
			#embed "shader/constrain.comp"
		};
		GLsizei sourceLength = sizeof(shaderSource);
		compileShader(shaderSource, sourceLength, &constrainComputeShader);
	}

	printf("Linking program 'constrainComputeProgram'...\n");
	GLuint constrainComputeProgram = glCreateProgram();
	glAttachShader(constrainComputeProgram, constrainComputeShader);
	linkProgram(&constrainComputeProgram);

	glUseProgram(circleShader);
	glUniform1i(glGetUniformLocation(circleShader, "positions"), 0);
	glUniform1f(glGetUniformLocation(circleShader, "radius"), PARTICLE_RADIUS);
	glUseProgram(0);

	float* positions = malloc(particles.count * sizeof(float) * 2);
	float* ssboData = malloc(particles.count * sizeof(float) * 4);

	glfwSetTime(0.0);
	float timePrev = 0.0f;
	float fpsTimer = 0.0f;
	int fpsCounter = 0;

	float timeDebt = 0.0f;

	while (!glfwWindowShouldClose(window)) {

		float timeCurr = glfwGetTime();
		float deltaTime = timeCurr - timePrev;
		timePrev = timeCurr;

		if (fpsTimer >= 1.0f) {
			printf("%d FPS\n", fpsCounter);
			fpsCounter = 0;
			fpsTimer -= 1.0f;
		}
		fpsTimer += deltaTime;
		fpsCounter++;

		// timeDebt += deltaTime;
		// while (timeDebt >= 0.0f) {
		// 	updateParticles(TIMESTEP);
		// 	timeDebt -= TIMESTEP;
		// }

		// zip together data for sending to GPU
		for (int i = 0; i < particles.count; i++) {
			positions[2*i] = particles.x[i];
			positions[2*i+1] = particles.y[i];
			ssboData[4*i] = particles.x[i];
			ssboData[4*i+1] = particles.y[i];
			ssboData[4*i+2] = particles.px[i];
			ssboData[4*i+3] = particles.py[i];
		}

		// update ssbo with particle data for compute shaders
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, particlesSSBO);
		glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, particles.count * 4 * sizeof(float), ssboData);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

		// dispatch compute shaders

		// compute movement
		glUseProgram(movementComputeProgram);
		glDispatchCompute(particles.count, 1, 1);
		// glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

		// compute collision
		glUseProgram(collisionComputeProgram);
		glDispatchCompute(particles.count, 1, 1);
		// glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

		// compute constraints
		glUseProgram(constrainComputeProgram);
		glDispatchCompute(particles.count, 1, 1);
		// glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_BUFFER_UPDATE_BARRIER_BIT);
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

		// read back compute shader data
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, particlesSSBO);
		glGetBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, particles.count * sizeof(float) * 4, ssboData);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
		for (int i = 0; i < particles.count; i++) {
			particles.x[i] = ssboData[4*i];
			particles.y[i] = ssboData[4*i+1];
			particles.px[i] = ssboData[4*i+2];
			particles.py[i] = ssboData[4*i+3];
		}

		// send particle data as 1D texture
		glUseProgram(circleShader);
		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_1D, positionTex);
		glTexSubImage1D(GL_TEXTURE_1D, 0, 0, particles.count, GL_RG, GL_FLOAT, positions);

		// make draw call
		glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		glDrawArraysInstanced(GL_TRIANGLES, 0, 6, particles.count);

		glBindTexture(GL_TEXTURE_1D, 0);
		glUseProgram(0);

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	glDeleteTextures(1, &positionTex);
	glfwDestroyWindow(window);
	glfwTerminate();
	freeParticles();
	freeGrid();
	free(positions);
	free(ssboData);

terminate:
	return exitcode;
}

void glfwErrorCallback(int code, const char* desc) {
	fprintf(stderr, "GLFW Error (%d): %s\n", code, desc);
}

void glfwCursorPosCallback(GLFWwindow* window, double x, double y) {
	mouse[0] = +(x / screen[0] - 0.5f);
	mouse[1] = -(y / screen[1] - 0.5f);
}

void glfwMouseButtonCallback(GLFWwindow* window, int button, int action, int mods) {
	if (button == GLFW_MOUSE_BUTTON_LEFT) {
		if (action == GLFW_PRESS) { mouse[2] = 1; }
		if (action == GLFW_RELEASE) { mouse[2] = 0; }
	}
	if (button == GLFW_MOUSE_BUTTON_RIGHT) {
		if (action == GLFW_PRESS) { mouse[3] = 1; }
		if (action == GLFW_RELEASE) { mouse[3] = 0; }
	}
}

void glfwFramebufferSizeCallback(GLFWwindow* window, int width, int height) {
	printf("Framebuffer: %d, %d\n", width, height);
	screen[0] = width;
	screen[1] = height;
	glViewport(0, 0, width, height);
}

