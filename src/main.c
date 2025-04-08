#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <pthread.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#define NUM_PARTICLES (1*8192)
#define INV_RADIUS 128
#define PARTICLE_RADIUS (1.0f / INV_RADIUS)
#define MOUSE_FORCE 16.0f
#define GRAVITY 8.0f
#define RESTITUTION 0.5f
#define DIST_EPSILON 0.00001f
#define SEP_FACTOR 0.49f
#define FIXED_TIMESTEP 0.0005
#define DO_COLLISION 1

#define SUBDIVISIONS 2
#define NUM_THREADS (1 << SUBDIVISIONS)

#define GRID_WIDTH INV_RADIUS
#define GRID_HEIGHT INV_RADIUS
#define CELL_CAP 8

#define RANDOM() (rand() / (float)RAND_MAX)
#define MAX_INFO_LOG 512

static float viewport[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
static float mouse[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

static struct {
	float curr[NUM_PARTICLES][2];
	float prev[NUM_PARTICLES][2];
} particles;

static struct {
	int keys[CELL_CAP];
	int count;
} grid[GRID_HEIGHT][GRID_WIDTH];

pthread_t threads[NUM_THREADS];
int threadIDs[NUM_THREADS];
int threadRegion[NUM_THREADS][4];
int threadPass = 0;

// void shuffleParticles(void) {
// 	for (int i = NUM_PARTICLES - 1; i > 0; i--) {
// 		int j = rand() % (i + 1);
// 		float tempX = particles.curr[i][0];
// 		float tempY = particles.curr[i][1];
// 		particles.curr[i][0] = particles.curr[j][0];
// 		particles.curr[i][1] = particles.curr[j][1];
// 		particles.curr[j][0] = tempX;
// 		particles.curr[j][1] = tempY;
// 		tempX = particles.prev[i][0];
// 		tempY = particles.prev[i][1];
// 		particles.prev[i][0] = particles.prev[j][0];
// 		particles.prev[i][1] = particles.prev[j][1];
// 		particles.prev[j][0] = tempX;
// 		particles.prev[j][1] = tempY;
// 	}
// }

void cellAppend(int key, int x, int y) {
	if (grid[y][x].count >= CELL_CAP) {
		// fprintf(stderr, "Overfull cell!\n");
		return;
	}
	grid[y][x].keys[grid[y][x].count++] = key;
}

void collideParticles(int i, int j) {
	float x1 = particles.curr[i][0];
	float y1 = particles.curr[i][1];
	float x2 = particles.curr[j][0];
	float y2 = particles.curr[j][1];
	float px1 = particles.prev[i][0];
	float py1 = particles.prev[i][1];
	float px2 = particles.prev[j][0];
	float py2 = particles.prev[j][1];
	float vx1 = x1 - px1;
	float vy1 = y1 - py1;
	float vx2 = x2 - px2;
	float vy2 = y2 - py2;
	float dx = x1 - x2;
	float dy = y1 - y2;
	float rsum = PARTICLE_RADIUS + PARTICLE_RADIUS;
	float rsum2 = rsum * rsum;
	float dist2 = dx * dx + dy * dy;
	if (dist2 <= rsum2) {
		float dist = sqrtf(dist2);
		float nx, ny;
		if (dist >= DIST_EPSILON) {
			nx = dx / dist;
			ny = dy / dist;
		} else {
			nx = 0.0f;
			ny = 1.0f;
			// printf("Division by near-zero value! (%f)\n", dist);
			// return;
		}

		float overlap = rsum - dist;

		float sep1x = SEP_FACTOR * overlap * nx;
		float sep1y = SEP_FACTOR * overlap * ny;

		float sep2x = SEP_FACTOR * overlap * nx;
		float sep2y = SEP_FACTOR * overlap * ny;


		particles.curr[i][0] += sep1x;
		particles.curr[i][1] += sep1y;
		particles.curr[j][0] -= sep2x;
		particles.curr[j][1] -= sep2y;

		float vrelx = vx1 - vx2;
		float vrely = vy1 - vy2;
		float vreln = vrelx * nx + vrely * ny;

		if (vreln < 0.0f) {
			float impulse = -(1.0f + RESTITUTION) * vreln * 0.5f;
			vx1 += impulse * nx;
			vy1 += impulse * ny;
			vx2 -= impulse * nx;
			vy2 -= impulse * ny;
			particles.prev[i][0] = particles.curr[i][0] - vx1;
			particles.prev[i][1] = particles.curr[i][1] - vy1;
			particles.prev[j][0] = particles.curr[j][0] - vx2;
			particles.prev[j][1] = particles.curr[j][1] - vy2;
		}
	}
}

void* collisionThread(void* arg) {
    int threadID = *(int*)arg;
	int x0 = threadRegion[threadID][0];
	int x1 = threadRegion[threadID][1];
	int y0 = threadRegion[threadID][2];
	int y1 = threadRegion[threadID][3];
	int mx = x0 + (x1 - x0) / 2;
	int my = y0 + (y1 - y0) / 2;
	switch (threadPass) {
		case 0: { x1 = mx; y1 = my; break; } // top left
		case 1: { x0 = mx + 1; y1 = my; break; } // top right
		case 2: { x1 = mx; y0 = my + 1; break; } // bottom left
		case 3: { x0 = mx + 1; y0 = my + 1; break; } // bottom right
	}
	// printf("Thread %d started on region (x0: %d, y0: %d) to (x1: %d, y1: %d)\n", threadID, x0, y0, x1, y1);
	for (int y = y0; y <= y1; y++) {
		for (int x = x0; x <= x1; x++) {
			int keys[9 * CELL_CAP];
			int count = 0;
			for (int dy = -1; dy <= 1; dy++) {
				for (int dx = -1; dx <= 1; dx++) {
					for (int ci = 0; ci < grid[y+dy][x+dx].count; ci++) {
						keys[count++] = grid[y+dy][x+dx].keys[ci];
					}
				}
			}
			for (int i = 0; i < count - 1; i++) {
				int key1 = keys[i];
				for (int j = i + 1; j < count; j++) {
					int key2 = keys[j];
					collideParticles(key1, key2);
				}
			}
		}
	}
	// pthread_exit(NULL);
	return NULL;
}

int spawnThreadsRecursive(int x0, int x1, int y0, int y1, int subdivs, int axis, int threadID) {
	if (x1 - x0 + 1 < 3 || y1 - y0 + 1 < 3) {
		fprintf(stderr, "Subdivided region too small!\n");
		exit(1);
	}
	if (subdivs == 0) {
		threadRegion[threadID][0] = x0;
		threadRegion[threadID][1] = x1;
		threadRegion[threadID][2] = y0;
		threadRegion[threadID][3] = y1;
		threadIDs[threadID] = threadID;
		// printf("Spawning thread (ID: %d) on region (x0: %d, y0: %d) to (x1: %d, y1: %d)\n", threadID, x0, y0, x1, y1);
		pthread_create(&threads[threadID], NULL, collisionThread, &threadIDs[threadID]);
		return 1;
	}
	int n = 0;
	if (axis == 0) {
		int m = x0 + (x1 - x0) / 2;
		n += spawnThreadsRecursive(x0, m, y0, y1, subdivs - 1, 1, threadID + n);
		n += spawnThreadsRecursive(m + 1, x1, y0, y1, subdivs - 1, 1, threadID + n);
	} else {
		int m = y0 + (y1 - y0) / 2;
		n += spawnThreadsRecursive(x0, x1, y0, m, subdivs - 1, 0, threadID + n);
		n += spawnThreadsRecursive(x0, x1, m + 1, y1, subdivs - 1, 0, threadID + n);
	}
	return n;
}

void compileShaderSource(GLsizei n, GLchar const* const* sources, GLint const* lengths, GLuint* shader) {
	glShaderSource(*shader, n, sources, lengths);
	glCompileShader(*shader);
	GLint compiled;
	glGetShaderiv(*shader, GL_COMPILE_STATUS, &compiled);
	if (compiled != GL_TRUE) {
		GLchar info[MAX_INFO_LOG];
		GLsizei len;
		glGetShaderInfoLog(*shader, sizeof(info), &len, info);
		fprintf(stderr, "Failed to compile shader:\n%.*s\n", len, info);
		exit(1);
	}
}

void compileShaderFiles(GLsizei n, char const* const* filenames, GLuint* shader) {
	GLchar** sources = malloc(n * sizeof(GLchar*));
	GLint* lengths = malloc(n * sizeof(GLint));
	for (size_t i = 0; i < n; i++) {
		const char* filename = filenames[i];
		printf("Sourcing file '%s'...\n", filename);
		FILE* file = fopen(filename, "r");
		if (!file) {
			fprintf(stderr, "Failed open '%s'\n", filename);
			exit(1);
		}
		fseek(file, 0, SEEK_END);
		long filesize = ftell(file);
		GLchar* source = malloc(filesize * sizeof(GLchar));
		fseek(file, 0, SEEK_SET);
		size_t length = fread(source, sizeof(GLchar), filesize, file);
		fclose(file);
		sources[i] = source;
		lengths[i] = length;
	}
	printf("Compiling shader...\n");
	compileShaderSource(n, (const GLchar* const*)sources, lengths, shader);
	for (size_t i = 0; i < n; i++) {
		free(sources[i]);
	}
	free(sources);
	free(lengths);
}

void linkShaderProgram(GLuint* program) {
	glLinkProgram(*program);
	GLint linked;
	glGetProgramiv(*program, GL_LINK_STATUS, &linked);
	if (linked != GL_TRUE) {
		GLchar info[MAX_INFO_LOG];
		GLsizei len;
		glGetProgramInfoLog(*program, sizeof(info), &len, info);
		fprintf(stderr, "Failed to link program:\n%.*s\n", len, info);
		exit(1);
	}
}

void initSimulation(void) {
	for (int i = 0; i < NUM_PARTICLES; i++) {
		float x = 2.0f * RANDOM() - 1.0f;
		float y = 2.0f * RANDOM() - 1.0f;
		float dx = 0.001f * (2.0f * RANDOM() - 1.0f);
		float dy = 0.001f * (2.0f * RANDOM() - 1.0f);
		particles.curr[i][0] = x;
		particles.curr[i][1] = y;
		particles.prev[i][0] = x - dx;
		particles.prev[i][1] = y - dy;
	}
}

void updateSimulation(float dt1, float dt2) {
	// Move with verlet integration
	for (int i = 0; i < NUM_PARTICLES; i++) {
		float x = particles.curr[i][0];
		float y = particles.curr[i][1];
		float px = particles.prev[i][0];
		float py = particles.prev[i][1];
		float dx = x - px;
		float dy = y - py;
		float ax = 0.0f;
		float ay = 0.0f;
		ax += mouse[2] * MOUSE_FORCE * (mouse[0] - x);
		ay += mouse[2] * MOUSE_FORCE * (mouse[1] - y);
		ax -= mouse[3] * MOUSE_FORCE * (mouse[0] - x);
		ay -= mouse[3] * MOUSE_FORCE * (mouse[1] - y);
		ay -= GRAVITY;
		particles.prev[i][0] = x;
		particles.prev[i][1] = y;
		particles.curr[i][0] = x + dx*dt1 + ax*dt2;
		particles.curr[i][1] = y + dy*dt1 + ay*dt2;
	};

#if DO_COLLISION
	// Sort particles by cell
	// static int countSortCounts[GRID_WIDTH * GRID_HEIGHT];
	// static int countSortPrefix[GRID_WIDTH * GRID_HEIGHT + 1];
	// static int countSortKey[NUM_PARTICLES];
	// static float tempCurr[NUM_PARTICLES][2];
	// static float tempPrev[NUM_PARTICLES][2];
	//
	// memset(countSortCounts, 0, sizeof(countSortCounts));
	// for (int i = 0; i < NUM_PARTICLES; i++) {
	// 	float x = particles.curr[i][0];
	// 	float y = particles.curr[i][1];
	// 	int cx = (int)((x + 1.0f) * 0.5f * GRID_WIDTH);
	// 	cx = cx < 0 ? 0 : cx >= GRID_WIDTH ? GRID_WIDTH - 1 : cx;
	// 	int cy = (int)((y + 1.0f) * 0.5f * GRID_HEIGHT);
	// 	cy = cy < 0 ? 0 : cy >= GRID_HEIGHT ? GRID_HEIGHT - 1 : cy;
	// 	int k = cy * GRID_WIDTH + cx;
	// 	countSortKey[i] = k;
	// 	countSortCounts[k]++;
	// }
	//
	// countSortPrefix[0] = 0;
	// for (int k = 0; k < GRID_WIDTH * GRID_HEIGHT; k++) {
	// 	countSortPrefix[k + 1] = countSortPrefix[k] + countSortCounts[k];
	// }
	//
	// for (int i = 0; i < NUM_PARTICLES; i++) {
	// 	int k = countSortKey[i];
	// 	int pos = countSortPrefix[k]++;
	// 	tempCurr[pos][0] = particles.curr[i][0];
	// 	tempCurr[pos][1] = particles.curr[i][1];
	// 	tempPrev[pos][0] = particles.prev[i][0];
	// 	tempPrev[pos][1] = particles.prev[i][1];
	// }
	//
	// memcpy(particles.curr, tempCurr, NUM_PARTICLES * sizeof(float[2]));
	// memcpy(particles.prev, tempPrev, NUM_PARTICLES * sizeof(float[2]));

	// Clear grid
	for (int y = 0; y < GRID_HEIGHT; y++) {
		for (int x = 0; x < GRID_WIDTH; x++) {
			grid[y][x].count = 0;
		}
	}

	// Populate grid with particles
	for (int i = 0; i < NUM_PARTICLES; i++) {
		float x = particles.curr[i][0];
		float y = particles.curr[i][1];
		int cx = (int)((x + 1.0f) * 0.5f * GRID_WIDTH);
		cx = cx < 0 ? 0 : cx >= GRID_WIDTH ? GRID_WIDTH - 1 : cx;
		int cy = (int)((y + 1.0f) * 0.5f * GRID_HEIGHT);
		cy = cy < 0 ? 0 : cy >= GRID_HEIGHT ? GRID_HEIGHT - 1 : cy;
		cellAppend(i, cx, cy);
	}

	// Partition the grid into regions of separate threads
	for (threadPass = 0; threadPass < 4; threadPass++) {
		int threadsSpawned = spawnThreadsRecursive(1, GRID_WIDTH - 2, 1, GRID_HEIGHT - 2, SUBDIVISIONS, 0, 0);
		// printf("%d threads spawned\n", threadsSpawned);
		// Wait for threads to finish
		for (int i = 0; i < threadsSpawned; i++) {
			pthread_join(threads[i], NULL);
		}
		// printf("All %d threads are done\n", threadsSpawned);
	}

#endif

	// Apply constraints
	for (int i = 0; i < NUM_PARTICLES; i++) {
		float x = particles.curr[i][0];
		float y = particles.curr[i][1];
		if (x < -1.0f+PARTICLE_RADIUS) x = -1.0f+PARTICLE_RADIUS;
		if (y < -1.0f+PARTICLE_RADIUS) y = -1.0f+PARTICLE_RADIUS;
		if (x > 1.0f-PARTICLE_RADIUS) x = 1.0f-PARTICLE_RADIUS;
		if (y > 1.0f-PARTICLE_RADIUS) y = 1.0f-PARTICLE_RADIUS;
		// float dist2 = x * x + y * y;
		// float dist = sqrtf(dist2);
		// float maxDist = 0.9f - PARTICLE_RADIUS;
		// float minDist = PARTICLE_RADIUS + 0.3f;
		// float nx = x / dist;
		// float ny = y / dist;
		// dist = fmaxf(fminf(dist, maxDist), minDist);
		// particle.curr[i][0] = nx * dist;
		// particle.curr[i][1] = ny * dist;
		particles.curr[i][0] = x;
		particles.curr[i][1] = y;
	}
}

void glfwErrorCallback(int code, const char* desc);
void glfwCursorPosCallback(GLFWwindow* window, double x, double y);
void glfwMouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
void glfwFramebufferSizeCallback(GLFWwindow* window, int width, int height);

int main(void) {

	srand(time(NULL));

	glfwInitHint(GLFW_WAYLAND_LIBDECOR, GLFW_WAYLAND_DISABLE_LIBDECOR);

	if (!glfwInit()) {
		fprintf(stderr, "Failed to initialize GLFW\n");
		exit(1);
	}

	printf("GLFW %s\n", glfwGetVersionString());

	glfwSetErrorCallback(glfwErrorCallback);

	// glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	GLFWwindow* window = glfwCreateWindow(1024, 1024, "Verlet Integration", NULL, NULL);
	if (!window) {
		fprintf(stderr, "Failed to create GLFW window\n");
		exit(1);
	}

	glfwSetFramebufferSizeCallback(window, glfwFramebufferSizeCallback);
	glfwSetCursorPosCallback(window, glfwCursorPosCallback);
	glfwSetMouseButtonCallback(window, glfwMouseButtonCallback);

	glfwMakeContextCurrent(window);

	glfwSwapInterval(0); // VSync

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
		fprintf(stderr, "Failed to load GLAD\n");
		exit(1);
	}

	printf("OpenGL %s\n", glGetString(GL_VERSION));

	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	compileShaderFiles(1, (const char* []) { "shader/point.vert" }, &vertexShader);

	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	compileShaderFiles(1, (const char* []) { "shader/point.frag" }, &fragmentShader);

	GLuint shaderProgram = glCreateProgram();
	glAttachShader(shaderProgram, vertexShader);
	glAttachShader(shaderProgram, fragmentShader);
	linkShaderProgram(&shaderProgram);

	glDetachShader(shaderProgram, vertexShader);
	glDetachShader(shaderProgram, fragmentShader);
	glDeleteShader(vertexShader);
	glDeleteShader(fragmentShader);

	glUseProgram(shaderProgram);
	glUniform1f(glGetUniformLocation(shaderProgram, "radius"), PARTICLE_RADIUS);
	glUseProgram(0);

	GLuint vao, vbo;
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, NUM_PARTICLES * sizeof(GLfloat[2]), NULL, GL_DYNAMIC_DRAW);

	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);
	glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 0, (void*)0);
	glEnableVertexAttribArray(0);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	// glEnable(GL_POINT_SPRITE_NV);
	glEnable(GL_POINT_SPRITE_ARB);

	glfwSetTime(0.0);
	float timePrev = 0.0f;
	float deltaTimePrev = 1.0f;
	float secTimer = 0.0f;
	int fpsCounter = 0;

	float spawnTimer = 0.0f;

	// float timestepTimer = 0.0f;

	initSimulation();

	printf("Running on %d threads\n", NUM_THREADS);

	while (!glfwWindowShouldClose(window)) {

		float timeCurr = glfwGetTime();
		float deltaTime = timeCurr - timePrev;
		timePrev = timeCurr;
		float dt1 = deltaTime / deltaTimePrev;
		float dt2 = 0.5f * deltaTime * (deltaTime + deltaTimePrev);
		deltaTimePrev = deltaTime;
		if (secTimer >= 1.0f) {
			printf("%d FPS\n", fpsCounter);
			fpsCounter = 0;
			secTimer -= 1.0f;
		}
		secTimer += deltaTime;
		fpsCounter++;

		// updateSimulation(dt1, dt2);
		updateSimulation(1.0, FIXED_TIMESTEP*FIXED_TIMESTEP);

		// Send particle positions to GPU
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferSubData(GL_ARRAY_BUFFER, 0, NUM_PARTICLES * sizeof(GLfloat[2]), particles.curr);
		glBindBuffer(GL_ARRAY_BUFFER, 0);

		// Make draw call
		glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		glBindVertexArray(vao);
		glUseProgram(shaderProgram);
		glDrawArrays(GL_POINTS, 0, NUM_PARTICLES);

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	glDeleteBuffers(1, &vbo);
	glDeleteVertexArrays(1, &vao);
	glDeleteProgram(shaderProgram);

	glfwDestroyWindow(window);
	glfwTerminate();

	return 0;
}

void glfwErrorCallback(int code, const char* desc) {
	fprintf(stderr, "GLFW Error (%d): %s\n", code, desc);
}

void glfwCursorPosCallback(GLFWwindow* window, double x, double y) {
	mouse[0] = +(2.0f * (x - viewport[2]) / viewport[0] - 1.0f);
	mouse[1] = -(2.0f * (y - viewport[3]) / viewport[1] - 1.0f);
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
	int viewportWidth;
	int viewportHeight;
	int viewportX;
	int viewportY;
	if (width == height) {
		viewportWidth = width;
		viewportHeight = height;
		viewportX = 0;
		viewportY = 0;
	}
	if (width < height) {
		viewportWidth = width;
		viewportHeight = width;
		viewportX = 0;
		viewportY = (height - viewportHeight) / 2;
	}
	if (width > height) {
		viewportWidth = height;
		viewportHeight = height;
		viewportX = (width - viewportWidth) / 2;
		viewportY = 0;
	}
	viewport[0] = viewportWidth;
	viewport[1] = viewportHeight;
	viewport[2] = viewportX;
	viewport[3] = viewportY;
	glViewport(viewportX, viewportY, viewportWidth, viewportHeight);
	glPointSize(PARTICLE_RADIUS * viewport[1]);
}

