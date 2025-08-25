#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <pthread.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#define NUM_PARTICLES (1024*8)
#define INV_RADIUS 128
#define PARTICLE_RADIUS (1.0f / INV_RADIUS)
#define MOUSE_FORCE 128.0f
#define GRAVITY 32.0f
#define RESTITUTION 0.5f
#define DIST_EPSILON 0.0000001f
#define SEP_FACTOR 0.3f
#define FIXED_TIMESTEP 0.0001
#define DO_COLLISION 1

#define SUBDIVISIONS 3
#define NUM_THREADS (1 << SUBDIVISIONS)

#define GRID_WIDTH INV_RADIUS
#define GRID_HEIGHT INV_RADIUS
#define CELL_CAP 8

#define RANDOM() (rand() / (float)RAND_MAX)
#define MAX_INFO_LOG 512

static float viewport[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
static float mouse[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

typedef struct {
	float x, y, px, py;
} Particle;

void collideParticlePair(const Particle* p1, const Particle* p2, float delta1[2], float delta2[2]) {
	float x1 = p1->x;
	float y1 = p1->y;
	float x2 = p2->x;
	float y2 = p2->y;
	float px1 = p1->px;
	float py1 = p1->py;
	float px2 = p2->px;
	float py2 = p2->py;
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

		delta1[0] += sep1x;
		delta1[1] += sep1y;
		delta2[0] -= sep2x;
		delta2[1] -= sep2y;

		// float vrelx = vx1 - vx2;
		// float vrely = vy1 - vy2;
		// float vreln = vrelx * nx + vrely * ny;
		//
		// if (vreln < 0.0f) {
		// 	float impulse = -(1.0f + RESTITUTION) * vreln * 0.5f;
		// 	vx1 += impulse * nx;
		// 	vy1 += impulse * ny;
		// 	vx2 -= impulse * nx;
		// 	vy2 -= impulse * ny;
		// 	p1->px = p1->x - vx1;
		// 	p1->py = p1->y - vy1;
		// 	p2->px = p2->x - vx2;
		// 	p2->py = p2->y - vy2;
		// }
	}
}

void collideCellSelf (
	int cellIndex,
	const int cellStart[GRID_WIDTH * GRID_HEIGHT],
	const int cellCount[GRID_WIDTH * GRID_HEIGHT],
	const Particle particles[NUM_PARTICLES],
	float deltas[NUM_PARTICLES][2]
) {
	int n = cellCount[cellIndex];
	int start = cellStart[cellIndex];
	if (n <= 0 || start < 0) return; // empty
	for (int i = 0; i < n - 1; i++) {
		for (int j = i + 1; j < n; j++) {
			collideParticlePair(&particles[start+i], &particles[start+j], deltas[start+i], deltas[start+j]);
		}
	}
}

void collideCellPair (
	int cellIndex1,
	int cellIndex2,
	const int cellStart[GRID_WIDTH * GRID_HEIGHT],
	const int cellCount[GRID_WIDTH * GRID_HEIGHT],
	const Particle particles[NUM_PARTICLES],
	float deltas[NUM_PARTICLES][2]
) {
	int n1 = cellCount[cellIndex1];
	int n2 = cellCount[cellIndex2];
	int start1 = cellStart[cellIndex1];
	int start2 = cellStart[cellIndex2];
	if (n1 <= 0 || n2 <= 0 || start1 < 0 || start2 < 0) return; // empty
	for (int i1 = 0; i1 < n1; i1++) {
		for (int i2 = 0; i2 < n2; i2++) {
			collideParticlePair(&particles[start1+i1], &particles[start2+i2], deltas[start1+i1], deltas[start2+i2]);
		}
	}
}

typedef struct {
	int left, right, top, bottom;
	const int* cellStart;
	const int* cellCount;
	const Particle* particles;
	float deltas[NUM_PARTICLES][2];
} ThreadData;

typedef struct {
	pthread_mutex_t mutex;
	pthread_cond_t cond;
	int count;      // how many threads have reached the barrier so far
	int tripCount;  // how many threads are required to release
	int generation; // which "round" of the barrier we're in
} my_barrier_t;

int my_barrier_init(my_barrier_t *barrier, unsigned count) {
	if (count == 0) return -1;
	barrier->count = 0;
	barrier->tripCount = count;
	barrier->generation = 0;
	pthread_mutex_init(&barrier->mutex, NULL);
	pthread_cond_init(&barrier->cond, NULL);
	return 0;
}

int my_barrier_destroy(my_barrier_t *barrier) {
	pthread_mutex_destroy(&barrier->mutex);
	pthread_cond_destroy(&barrier->cond);
	return 0;
}

int my_barrier_wait(my_barrier_t *barrier) {
	pthread_mutex_lock(&barrier->mutex);

	int gen = barrier->generation;

	barrier->count++;
	if (barrier->count == barrier->tripCount) {
		// last thread to arrive: reset and wake everyone
		barrier->generation++;
		barrier->count = 0;
		pthread_cond_broadcast(&barrier->cond);
		pthread_mutex_unlock(&barrier->mutex);
		return 1; // special return for "serial thread"
	}

	while (gen == barrier->generation) {
		pthread_cond_wait(&barrier->cond, &barrier->mutex);
	}

	pthread_mutex_unlock(&barrier->mutex);
	return 0;
}

my_barrier_t barrier;

void* collisionThread(void* arg) {
    ThreadData* data = arg;
	int x0 = data->left;
	int x1 = data->right;
	int y0 = data->top;
	int y1 = data->bottom;
	const int* cellStart = data->cellStart;
	const int* cellCount = data->cellCount;
	const Particle* particles = data->particles;
	// printf("Thread active on region (x0: %d, y0: %d) to (x1: %d, y1: %d)\n", x0, y0, x1, y1);
	while (1) {
		my_barrier_wait(&barrier);
		memset(data->deltas, 0, sizeof(data->deltas));
		for (int y = y0; y <= y1; y++) {
			for (int x = x0; x <= x1; x++) {
				int cellIndex = y * GRID_WIDTH + x;
				for (int dy = -1; dy <= 1; dy++) {
					for (int dx = -1; dx <= 1; dx++) {
						int otherX = x + dx;
						int otherY = y + dy;
						if (otherX < 0 || otherX >= GRID_WIDTH || otherY < 0 || otherY >= GRID_HEIGHT) continue;
						int otherIndex = otherY * GRID_WIDTH + otherX;
						if (dx == 0 && dy == 0) {
							collideCellSelf(cellIndex, cellStart, cellCount, particles, data->deltas);
						} else {
							// only handle each unordered pair once
							if (otherIndex > cellIndex) {
								collideCellPair(cellIndex, otherIndex, cellStart, cellCount, particles, data->deltas);
							}
						}
					}
				}
			}
		}
		my_barrier_wait(&barrier);
	}
	return NULL;
}

int spawnThreadsRecursive (
	int x0, int x1, int y0, int y1,
	int subdivs, int axis, int threadID,
	pthread_t threads[NUM_THREADS],
	ThreadData threadData[NUM_THREADS]
) {
	if (x1 - x0 + 1 < 3 || y1 - y0 + 1 < 3) {
		fprintf(stderr, "Subdivided region too small!\n");
		exit(1);
	}
	if (subdivs == 0) {
		threadData[threadID].left = x0;
		threadData[threadID].right = x1;
		threadData[threadID].top = y0;
		threadData[threadID].bottom = y1;
		// printf("Spawning thread (ID: %d) on region (x0: %d, y0: %d) to (x1: %d, y1: %d)\n", threadID, x0, y0, x1, y1);
		pthread_create(&threads[threadID], NULL, collisionThread, &threadData[threadID]);
		return 1;
	}
	int n = 0;
	if (axis == 0) {
		int m = x0 + (x1 - x0) / 2;
		n += spawnThreadsRecursive(x0, m, y0, y1, subdivs - 1, 1, threadID + n, threads, threadData);
		n += spawnThreadsRecursive(m + 1, x1, y0, y1, subdivs - 1, 1, threadID + n, threads, threadData);
	} else {
		int m = y0 + (y1 - y0) / 2;
		n += spawnThreadsRecursive(x0, x1, y0, m, subdivs - 1, 0, threadID + n, threads, threadData);
		n += spawnThreadsRecursive(x0, x1, m + 1, y1, subdivs - 1, 0, threadID + n, threads, threadData);
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

void initParticle(Particle* p) {
	p->x = 2.0f * RANDOM() - 1.0f;
	p->y = 2.0f * RANDOM() - 1.0f;
	float dx = 0.001f * (2.0f * RANDOM() - 1.0f);
	float dy = 0.001f * (2.0f * RANDOM() - 1.0f);
	p->px = p->x - dx;
	p->py = p->y - dy;
}

void initParticles(Particle particles[NUM_PARTICLES]) {
	for (int i = 0; i < NUM_PARTICLES; i++) {
		initParticle(&particles[i]);
	}
}

// Move particle with verlet integration
void moveParticle(Particle* p, float dt1, float dt2) {
	float dx = p->x - p->px;
	float dy = p->y - p->py;
	float ax = 0.0f;
	float ay = 0.0f;
	ax += mouse[2] * MOUSE_FORCE * (mouse[0] - p->x);
	ay += mouse[2] * MOUSE_FORCE * (mouse[1] - p->y);
	ax -= mouse[3] * MOUSE_FORCE * (mouse[0] - p->x);
	ay -= mouse[3] * MOUSE_FORCE * (mouse[1] - p->y);
	ay -= GRAVITY;
	p->px = p->x;
	p->py = p->y;
	p->x += dx * dt1 + ax * dt2;
	p->y += dy * dt1 + ay * dt2;
}

void moveParticles(Particle particles[NUM_PARTICLES], float dt1, float dt2) {
	for (int i = 0; i < NUM_PARTICLES; i++) {
		moveParticle(&particles[i], dt1, dt2);
	}
}

int clamp(int x, int a, int b) {
	return x < a ? a : (x > b ? b : x);
}

float clampf(float x, float a, float b) {
	return x < a ? a : (x > b ? b : x);
}

int getCellIndex(const Particle* p) {
	int cellX = (int)((p->x + 1.0f) * 0.5f * GRID_WIDTH);
	int cellY = (int)((p->y + 1.0f) * 0.5f * GRID_HEIGHT);
	cellX = clamp(cellX, 0, GRID_WIDTH - 1);
	cellY = clamp(cellY, 0, GRID_HEIGHT - 1);
	return cellY * GRID_WIDTH + cellX;
}

void constrainParticle(Particle* p) {
	p->x = clampf(p->x, -1.0f+PARTICLE_RADIUS, 1.0f-PARTICLE_RADIUS);
	p->y = clampf(p->y, -1.0f+PARTICLE_RADIUS, 1.0f-PARTICLE_RADIUS);
	// float dist2 = x * x + y * y;
	// float dist = sqrtf(dist2);
	// float maxDist = 0.9f - PARTICLE_RADIUS;
	// float minDist = PARTICLE_RADIUS + 0.3f;
	// float nx = x / dist;
	// float ny = y / dist;
	// dist = fmaxf(fminf(dist, maxDist), minDist);
	// particle.curr[i][0] = nx * dist;
	// particle.curr[i][1] = ny * dist;
}

void constrainParticles(Particle particles[NUM_PARTICLES]) {
	for (int i = 0; i < NUM_PARTICLES; i++) {
		constrainParticle(&particles[i]);
	}
}

int compareCellIndex(const void* a, const void* b) {
    const Particle* p1 = a;
    const Particle* p2 = b;
    int i1 = getCellIndex(p1);
    int i2 = getCellIndex(p2);
    return i1 - i2;
}

void sortParticlesByCell(Particle particles[NUM_PARTICLES]) {
	qsort(particles, NUM_PARTICLES, sizeof(Particle), compareCellIndex);
}

void buildParticleGrid(Particle particles[NUM_PARTICLES],
	int cellStart[GRID_WIDTH * GRID_HEIGHT],
	int cellCount[GRID_WIDTH * GRID_HEIGHT]
	) {

	// zero counts
	for (int i = 0; i < GRID_WIDTH * GRID_HEIGHT; i++) {
		cellCount[i] = 0;
		cellStart[i] = -1;
	}

	// first pass: count particles per cell
	for (int i = 0; i < NUM_PARTICLES; i++) {
		int ci = getCellIndex(&particles[i]);
		if (ci < 0) continue; // if you allow out-of-bounds
		cellCount[ci]++;
	}

	// exclusive prefix sum over counts → cellStart[]
	int sum = 0;
	for (int i = 0; i < GRID_WIDTH * GRID_HEIGHT; i++) {
		if (cellCount[i] > 0) {
			cellStart[i] = sum;
		}
		sum += cellCount[i];
	}

	// second pass: scatter particles into a new array
	static Particle sorted[NUM_PARTICLES];   // or malloc/free if you prefer
	int offset[GRID_WIDTH * GRID_HEIGHT];                    // local index per cell
	for (int i = 0; i < GRID_WIDTH * GRID_HEIGHT; i++) {
		offset[i] = 0;
	}

	for (int i = 0; i < NUM_PARTICLES; i++) {
		int ci = getCellIndex(&particles[i]);
		int dst = cellStart[ci] + offset[ci]++;
		sorted[dst] = particles[i];
	}

	// copy back
	for (int i = 0; i < NUM_PARTICLES; i++) {
		particles[i] = sorted[i];
	}
}

void fillCellsWithParticles (
	int cellStart[GRID_WIDTH * GRID_HEIGHT],
	int cellCount[GRID_WIDTH * GRID_HEIGHT],
	const Particle particles[NUM_PARTICLES]) {
	for (int i = 0; i < NUM_PARTICLES; i++) {
		int cellIndex = getCellIndex(&particles[i]);
		cellCount[cellIndex]++;
		if (cellStart[cellIndex] == -1) {
			cellStart[cellIndex] = i;
		}
	}
}
void applyThreadDeltas(Particle particles[NUM_PARTICLES], ThreadData threadData[NUM_THREADS]) {
	for (int i = 0; i < NUM_THREADS; i++) {
		for (int j = 0; j < NUM_PARTICLES; j++) {
			particles[j].x += threadData[i].deltas[j][0];
			particles[j].y += threadData[i].deltas[j][1];
		}
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

	GLFWwindow* window = glfwCreateWindow(1024, 1024, "Verlet", NULL, NULL);
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

	Particle particles[NUM_PARTICLES];
	initParticles(particles);

	GLuint vao, vbo;
	glGenBuffers(1, &vbo);
	glBindBuffer(GL_ARRAY_BUFFER, vbo);
	glBufferData(GL_ARRAY_BUFFER, sizeof(particles), particles, GL_DYNAMIC_DRAW);

	glGenVertexArrays(1, &vao);
	glBindVertexArray(vao);
	glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, 0, (void*)0);
	glEnableVertexAttribArray(0);

	glBindBuffer(GL_ARRAY_BUFFER, 0);
	glBindVertexArray(0);

	glEnable(GL_POINT_SPRITE_ARB);

	glfwSetTime(0.0);
	float timePrev = 0.0f;
	float deltaTimePrev = 1.0f;
	float secTimer = 0.0f;
	int fpsCounter = 0;

	float spawnTimer = 0.0f;

	int cellCount[GRID_WIDTH * GRID_HEIGHT];
	int cellStart[GRID_WIDTH * GRID_HEIGHT];
	pthread_t threads[NUM_THREADS];
	ThreadData threadData[NUM_THREADS];
	for (int i = 0; i < NUM_THREADS; i++) {
		threadData[i].particles = particles;
		threadData[i].cellStart = cellStart;
		threadData[i].cellCount = cellCount;
	}

	my_barrier_init(&barrier, NUM_THREADS + 1);
	int threadsSpawned = spawnThreadsRecursive (
		0, GRID_WIDTH - 1, 0, GRID_HEIGHT - 1,
		SUBDIVISIONS, 0, 0,
		threads, threadData
	);
	printf("Running on %d threads\n", threadsSpawned);

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

		moveParticles(particles, 1.0, FIXED_TIMESTEP*FIXED_TIMESTEP);

		buildParticleGrid(particles, cellStart, cellCount);
		// memset(cellCount, 0, sizeof(cellCount));
		// memset(cellStart, -1, sizeof(cellStart));
		// sortParticlesByCell(particles);
		// fillCellsWithParticles(cellStart, cellCount, particles);

		my_barrier_wait(&barrier); // begin collision workers
		my_barrier_wait(&barrier); // wait for collision workers
		
		applyThreadDeltas(particles, threadData);

		constrainParticles(particles);

		// Send particle data to GPU
		glBindBuffer(GL_ARRAY_BUFFER, vbo);
		glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(particles), particles);
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

	// for (int i = 0; i < threadsSpawned; i++) {
	// 	pthread_join(threads[i], NULL);
	// }

	my_barrier_destroy(&barrier);

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

