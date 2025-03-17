#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>
#include <pthread.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#define NUM_PARTICLES 8192
#define INV_RADIUS 128
#define PARTICLE_RADIUS (1.0f / INV_RADIUS)
#define QUALITY 256
#define TIMESTEP (1.0f / QUALITY)
#define MOUSE_FORCE 16.0f
#define GRAVITY 4.0f
#define RESTITUTION 0.8f
#define MAX_INFO_LOG 512
#define DIST_EPSILON 0.000001f
#define RANDOM() (rand() / (float)RAND_MAX)

#define SUBDIVISIONS 3
#define NUM_THREADS (1 << SUBDIVISIONS)

#define GRID_WIDTH INV_RADIUS
#define GRID_HEIGHT INV_RADIUS
#define CELL_WIDTH (2.0f / GRID_WIDTH)
#define CELL_HEIGHT (2.0f / GRID_HEIGHT)

float viewport[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
float mouse[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

struct {
	float (*curr)[2];
	float (*prev)[2];
	// int* pressure;
	int* index;
} particle;

struct ListNode {
	int key;
	struct ListNode* next;
};

struct ListNode** grid;

pthread_t threads[NUM_THREADS];
int threadIDs[NUM_THREADS];
int threadRegion[NUM_THREADS][4];
int collisionPass = 0;

void insert(int key, struct ListNode* head) {
	struct ListNode* node = malloc(sizeof(struct ListNode));
	if (!node) {
		fprintf(stderr, "Failed to allocate node\n");
		exit(1);
	}
	node->key = key;
	node->next = head->next;
	head->next = node;
}

void listfree(struct ListNode* head) {
	while (head) {
		struct ListNode* next = head->next;
		head->key = -1;
		head->next = NULL;
		free(head);
		head = next;
	}
}

void collide(int i, int j) {
	float x1 = particle.curr[i][0];
	float y1 = particle.curr[i][1];
	float px1 = particle.prev[i][0];
	float py1 = particle.prev[i][1];
	float vx1 = x1 - px1;
	float vy1 = y1 - py1;
	float x2 = particle.curr[j][0];
	float y2 = particle.curr[j][1];
	float px2 = particle.prev[j][0];
	float py2 = particle.prev[j][1];
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
			printf("Division by near-zero value! (%f)\n", dist);
		}

		float overlap = rsum - dist;
		float factor1 = 0.5f;
		float factor2 = 0.5f;

		particle.curr[i][0] += factor1 * overlap * nx;
		particle.curr[i][1] += factor1 * overlap * ny;
		particle.curr[j][0] -= factor2 * overlap * nx;
		particle.curr[j][1] -= factor2 * overlap * ny;

		float vrelx = vx1 - vx2;
		float vrely = vy1 - vy2;
		float vreln = vrelx * nx + vrely * ny;

		if (vreln < 0.0f) {
			float impulse = -(1.0f + RESTITUTION) * vreln * 0.5f;
			vx1 += impulse * nx;
			vy1 += impulse * ny;
			vx2 -= impulse * nx;
			vy2 -= impulse * ny;
			particle.prev[i][0] = particle.curr[i][0] - vx1;
			particle.prev[i][1] = particle.curr[i][1] - vy1;
			particle.prev[j][0] = particle.curr[j][0] - vx2;
			particle.prev[j][1] = particle.curr[j][1] - vy2;
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
	switch (collisionPass) {
		case 0: { x1 = mx; y1 = my; break; } // top left
		case 1: { x0 = mx + 1; y1 = my; break; } // top right
		case 2: { x1 = mx; y0 = my + 1; break; } // bottom left
		case 3: { x0 = mx + 1; y0 = my + 1; break; } // bottom right
		default: {
			fprintf(stderr, "Invalid collision pass\n");
			exit(1);
		}
	}
	// printf("Thread %d started on region (x0: %d, y0: %d) to (x1: %d, y1: %d)\n", threadID, x0, y0, x1, y1);
	for (int y = y0; y <= y1; y++) {
		for (int x = x0; x <= x1; x++) {
			struct ListNode group;
			group.key = -1;
			group.next = NULL;
			for (int dy = -1; dy <= 1; dy++) {
				for (int dx = -1; dx <= 1; dx++) {
					struct ListNode* curr = grid[y+dy][x+dx].next;
					while (curr) {
						insert(curr->key, &group);
						curr = curr->next;
					}
				}
			}
			struct ListNode* left = group.next;
			while (left && left->next) {
				struct ListNode* right = left->next;
				while (right) {
					int i = left->key;
					int j = right->key;
					collide(i, j);
					right = right->next;
				}
				left = left->next;
			}
			listfree(group.next);
			// group.key = -1;
			// group.next = NULL;
		}
	}
	pthread_exit(NULL);
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

void glfwErrorCallback(int code, const char* desc);
void glfwCursorPosCallback(GLFWwindow* window, double x, double y);
void glfwMouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
void glfwFramebufferSizeCallback(GLFWwindow* window, int width, int height);

void initParticles(void) {
	particle.curr = malloc(NUM_PARTICLES * sizeof(float[2]));
	particle.prev = malloc(NUM_PARTICLES * sizeof(float[2]));
	particle.index = malloc(NUM_PARTICLES * sizeof(int));
	// particle.pressure = malloc(NUM_PARTICLES * sizeof(int));
	for (int i = 0; i < NUM_PARTICLES; i++) {
		float x = 2.0f * RANDOM() - 1.0f;
		float y = 2.0f * RANDOM() - 1.0f;
		float dx = 0.0f;
		float dy = 0.0f;
		particle.curr[i][0] = x;
		particle.curr[i][1] = y;
		particle.prev[i][0] = x - dx;
		particle.prev[i][1] = y - dy;
		particle.index[i] = i;
		// particle.pressure[i] = 0;
	}
}

void initGrid(void) {
	grid = malloc(GRID_HEIGHT * sizeof(struct ListNode*));
	for (int y = 0; y < GRID_HEIGHT; y++) {
		grid[y] = malloc(GRID_WIDTH * sizeof(struct ListNode));
		for (int x = 0; x < GRID_WIDTH; x++) {
			grid[y][x].key = -1;
			grid[y][x].next = NULL;
		}
	}
}

void freeParticles(void) {
	free(particle.curr);
	free(particle.prev);
	free(particle.index);
	// free(particle.pressure);
}

void freeGrid(void) {
	for (int y = 0; y < GRID_HEIGHT; y++) {
		free(grid[y]);
	}
	free(grid);
}

// int comparePressure(const void* a, const void* b) {
// 	int i1 = *(int*)a;
// 	int i2 = *(int*)b;
// 	return particle.pressure[i1] - particle.pressure[i2];
// }

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

	GLuint circleVertShader = glCreateShader(GL_VERTEX_SHADER);
	compileShaderFiles(1, (const char* []) { "shader/circle.vert" }, &circleVertShader);

	GLuint circleFragShader = glCreateShader(GL_FRAGMENT_SHADER);
	compileShaderFiles(1, (const char* []) { "shader/circle.frag" }, &circleFragShader);

	GLuint renderProgram = glCreateProgram();
	glAttachShader(renderProgram, circleVertShader);
	glAttachShader(renderProgram, circleFragShader);
	linkShaderProgram(&renderProgram);
	glUseProgram(renderProgram);
	glUniform1f(glGetUniformLocation(renderProgram, "radius"), PARTICLE_RADIUS);

	GLuint ssbo;
	glGenBuffers(1, &ssbo);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssbo);
	glBufferData(GL_SHADER_STORAGE_BUFFER, NUM_PARTICLES * sizeof(GLfloat[2]), NULL, GL_DYNAMIC_DRAW);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	glfwSetTime(0.0);
	float timePrev = 0.0f;
	float secTimer = 0.0f;
	int fpsCounter = 0;

	float timestepTimer = 0.0f;

	initParticles();
	initGrid();

	printf("Running on %d threads\n", NUM_THREADS);

	while (!glfwWindowShouldClose(window)) {

		float timeCurr = glfwGetTime();
		float deltaTime = timeCurr - timePrev;
		timePrev = timeCurr;

		if (secTimer >= 1.0f) {
			printf("%d FPS\n", fpsCounter);
			fpsCounter = 0;
			secTimer -= 1.0f;
		}
		secTimer += deltaTime;
		fpsCounter++;

		timestepTimer += deltaTime;

		while (timestepTimer >= TIMESTEP) {
			timestepTimer -= TIMESTEP;
			float dt = TIMESTEP;

			// Move particles with verlet integration
			for (int i = 0; i < NUM_PARTICLES; i++) {
				float x = particle.curr[i][0];
				float y = particle.curr[i][1];
				float px = particle.prev[i][0];
				float py = particle.prev[i][1];
				float dx = x - px;
				float dy = y - py;
				float ax = 0.0f;
				float ay = 0.0f;
				ax += mouse[2] * MOUSE_FORCE * (mouse[0] - x);
				ay += mouse[2] * MOUSE_FORCE * (mouse[1] - y);
				ay -= GRAVITY;
				particle.prev[i][0] = x;
				particle.prev[i][1] = y;
				particle.curr[i][0] = x + dx + ax*dt*dt;
				particle.curr[i][1] = y + dy + ay*dt*dt;
			}

			// // Calculate "pressure" by counting touching particles
			// for (int i = 0; i < NUM_PARTICLES; i++) {
			// 	int pressure = 0;
			// 	float x1 = particle.curr[i][0];
			// 	float y1 = particle.curr[i][1];
			// 	for (int j = 0; j < NUM_PARTICLES; j++) {
			// 		float x2 = particle.curr[j][0];
			// 		float y2 = particle.curr[j][1];
			// 		float dx = x1 - x2;
			// 		float dy = y1 - y2;
			// 		float rsum = PARTICLE_RADIUS + PARTICLE_RADIUS;
			// 		float rsum2 = rsum * rsum;
			// 		float dist2 = dx * dx + dy * dy;
			// 		if (dist2 <= rsum2) {
			// 			pressure++;
			// 		}
			// 	}
			// 	particle.pressure[i] = pressure;
			// }

			// // Sort particles by pressure
			// qsort(particle.index, NUM_PARTICLES, sizeof(int), comparePressure);

			// Clear all previous grid data
			for (int y = 0; y < GRID_HEIGHT; y++) {
				for (int x = 0; x < GRID_WIDTH; x++) {
					listfree(grid[y][x].next);
					grid[y][x].next = NULL;
				}
			}

			// Populate grid data with particles
			for (int i = 0; i < NUM_PARTICLES; i++) {
				float x = particle.curr[i][0];
				float y = particle.curr[i][1];
				int gridX = (x + 1.0f) * 0.5f * GRID_WIDTH;
				int gridY = (y + 1.0f) * 0.5f * GRID_HEIGHT;
				gridX = gridX < 0 ? 0 : gridX;
				gridX = gridX >= GRID_WIDTH ? GRID_WIDTH - 1 : gridX;
				gridY = gridY < 0 ? 0 : gridY;
				gridY = gridY >= GRID_HEIGHT ? GRID_HEIGHT - 1 : gridY;
				if (gridX >= 0 && gridX < GRID_WIDTH && gridY >= 0 && gridY < GRID_HEIGHT) {
					struct ListNode* head = &grid[gridY][gridX];
					// printf("Inserting particle into cell (%d, %d)\n", gridX, gridY);
					insert(i, head);
				} else {
					fprintf(stderr, "Grid index out of range! (%d, %d)\n", gridX, gridY);
					exit(1);
				}
			}

			// Partition the grid into regions of separate threads
			for (collisionPass = 0; collisionPass < 4; collisionPass++) {
				int threadsSpawned = spawnThreadsRecursive(1, GRID_WIDTH - 2, 1, GRID_HEIGHT - 2, SUBDIVISIONS, 0, 0);
				// printf("%d threads spawned\n", threadsSpawned);
				// Wait for collision threads to finish
				for (int i = 0; i < threadsSpawned; i++) {
					pthread_join(threads[i], NULL);
				}
				// printf("All %d threads are done\n", threadsSpawned);
			}

			// Apply maximum distance constraint
			for (int i = 0; i < NUM_PARTICLES; i++) {
				float x = particle.curr[i][0];
				float y = particle.curr[i][1];
				float dist2 = x * x + y * y;
				float dist = sqrtf(dist2);
				float maxDist = 1.0f - PARTICLE_RADIUS;
				if (dist > maxDist) {
					float nx = x / dist;
					float ny = y / dist;
					particle.curr[i][0] = nx * maxDist;
					particle.curr[i][1] = ny * maxDist;
				}
			}
		}

		// Send particle positions to GPU
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssbo);
		glBufferSubData(GL_SHADER_STORAGE_BUFFER, 0, NUM_PARTICLES * sizeof(GLfloat[2]), particle.curr);
		glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

		// Make draw call
		glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		glDrawArraysInstanced(GL_TRIANGLES, 0, 6, NUM_PARTICLES);

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

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
}

