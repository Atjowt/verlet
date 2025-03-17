#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#define NUM_PARTICLES 1024
#define MAX_INFO_LOG 512
#define SUBSTEPS 8
#define RANDOM() (rand() / (float)RAND_MAX)

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

float viewport[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
float mouse[4] = { 0.0f, 0.0f, 0.0f, 0.0f };

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
		fprintf(stderr, "Failed to initialize GLAD\n");
		exit(1);
	}

	printf("OpenGL %s\n", glGetString(GL_VERSION));

	GLuint circleVertShader = glCreateShader(GL_VERTEX_SHADER);
	const char* circleVertShaderFiles[] = { "shader/common.glsl", "shader/circle.vert" };
	compileShaderFiles(2, circleVertShaderFiles, &circleVertShader);

	GLuint circleFragShader = glCreateShader(GL_FRAGMENT_SHADER);
	const char* circleFragShaderFiles[] = { "shader/common.glsl", "shader/circle.frag" };
	compileShaderFiles(2, circleFragShaderFiles, &circleFragShader);

	GLuint renderProgram = glCreateProgram();
	glAttachShader(renderProgram, circleVertShader);
	glAttachShader(renderProgram, circleFragShader);
	linkShaderProgram(&renderProgram);

	GLuint computeMovementShader = glCreateShader(GL_COMPUTE_SHADER);
	const char* computeMovementShaderFiles[] = { "shader/common.glsl", "shader/movement.comp" };
	compileShaderFiles(2, computeMovementShaderFiles, &computeMovementShader);
	GLuint movementProgram = glCreateProgram();
	glAttachShader(movementProgram, computeMovementShader);
	linkShaderProgram(&movementProgram);

	// GLuint computeGridShader = glCreateShader(GL_COMPUTE_SHADER);
	// const char* computeGridShaderFiles[] = { "shader/common.glsl", "shader/grid.comp" };
	// compileShaderFiles(2, computeGridShaderFiles, &computeGridShader);
	// GLuint gridProgram = glCreateProgram();
	// glAttachShader(gridProgram, computeGridShader);
	// linkShaderProgram(&gridProgram);

	GLuint computeCollisionShader = glCreateShader(GL_COMPUTE_SHADER);
	const char* computeCollisionShaderFiles[] = { "shader/common.glsl", "shader/collision.comp" };
	compileShaderFiles(2, computeCollisionShaderFiles, &computeCollisionShader);
	GLuint collisionProgram = glCreateProgram();
	glAttachShader(collisionProgram, computeCollisionShader);
	linkShaderProgram(&collisionProgram);

	GLuint computeSeparateShader = glCreateShader(GL_COMPUTE_SHADER);
	const char* computeSeparateShaderFiles[] = { "shader/common.glsl", "shader/separate.comp" };
	compileShaderFiles(2, computeSeparateShaderFiles, &computeSeparateShader);
	GLuint separateProgram = glCreateProgram();
	glAttachShader(separateProgram, computeSeparateShader);
	linkShaderProgram(&separateProgram);

	float (*initPos)[4] = malloc(NUM_PARTICLES * sizeof(GLfloat[4]));
	for (int i = 0; i < NUM_PARTICLES; i++) {
		float x = 2.0f * RANDOM() - 1.0f;
		float y = 2.0f * RANDOM() - 1.0f;
		float dx = 0.0f;
		float dy = 0.0f;
		initPos[i][0] = x;
		initPos[i][1] = y;
		initPos[i][2] = x - dx;
		initPos[i][3] = y - dy;
	}

	GLuint particleBuffer;
	glGenBuffers(1, &particleBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, particleBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, NUM_PARTICLES * sizeof(GLfloat[4]), initPos, GL_DYNAMIC_DRAW);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, particleBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);
	free(initPos);

	GLuint separationsBuffer;
	glGenBuffers(1, &separationsBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, separationsBuffer);
	glBufferData(GL_SHADER_STORAGE_BUFFER, NUM_PARTICLES * sizeof(GLfloat[2]), NULL, GL_DYNAMIC_DRAW);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, separationsBuffer);
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

	// GLuint counterBuffer;
	// glGenBuffers(1, &counterBuffer);
	// glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, counterBuffer);
	// glBufferData(GL_ATOMIC_COUNTER_BUFFER, gridSize * gridSize * sizeof(GLuint), NULL, GL_DYNAMIC_DRAW);
	// glBindBufferBase(GL_ATOMIC_COUNTER_BUFFER, 2, counterBuffer);
	// glBindBuffer(GL_ATOMIC_COUNTER_BUFFER, 0);

	glfwSetTime(0.0);
	float timePrev = 0.0f;
	float fpsTimer = 0.0f;
	int fpsCounter = 0;

	GLint shaderTime = glGetUniformLocation(movementProgram, "time");
	GLint shaderMouse = glGetUniformLocation(movementProgram, "mouse");

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

		float timestep = deltaTime / SUBSTEPS;
		for (int step = 0; step < SUBSTEPS; step++) {
			glUseProgram(movementProgram);
			glUniform2f(shaderTime, timeCurr, timestep);
			glUniform4fv(shaderMouse, 1, mouse);
			glDispatchCompute(NUM_PARTICLES, 1, 1);
			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

			glUseProgram(collisionProgram);
			glDispatchCompute(NUM_PARTICLES, 1, 1);
			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);

			glUseProgram(separateProgram);
			glDispatchCompute(NUM_PARTICLES, 1, 1);
			glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT);
		}

		glUseProgram(renderProgram);
		glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		glDrawArraysInstanced(GL_TRIANGLES, 0, 6, NUM_PARTICLES);

		// glBindBuffer(GL_SHADER_STORAGE_BUFFER, countsBuffer);
		// // GLuint* countsMap = glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_WRITE);
		// GLuint* countsMap = glMapBuffer(GL_SHADER_STORAGE_BUFFER, GL_READ_ONLY);
		// if (!countsMap) {
		// 	GLenum err = glGetError();
		// 	fprintf(stderr, "MAP BUFFER ERROR: %s (%d)\n", glGetString(err), err);
		// 	exit(1);
		// }
		// int totalCount = 0;
		// printf("-----------------\n");
		// for (int y = 0; y < gridSize; y++) {
		// 	for (int x = 0; x < gridSize; x++) {
		// 		int i = (gridSize - y - 1) * gridSize + x;
		// 		GLuint count = countsMap[i];
		// 		totalCount += count;
		// 		printf("%u ", count);
		// 	}
		// 	printf("\n");
		// }
		// printf("-----------------\n");
		// if (totalCount != NUM_PARTICLES) {
		// 	fprintf(stderr, "COUNT DOESNT ADD UP\n");
		// 	// exit(1);
		// }
		// glUnmapBuffer(GL_SHADER_STORAGE_BUFFER);
		// glBindBuffer(GL_SHADER_STORAGE_BUFFER, 0);

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

