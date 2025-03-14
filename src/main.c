#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#define INSTANCES 100000
#define MAX_INFO_LOG 512

#define random() (rand() / (float)RAND_MAX)

void glfwErrorCallback(int code, const char* desc);
void glfwCursorPosCallback(GLFWwindow* window, double x, double y);
void glfwMouseButtonCallback(GLFWwindow* window, int button, int action, int mods);
void glfwFramebufferSizeCallback(GLFWwindow* window, int width, int height);

float mouse[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
float screen[2] = { 0.0f, 0.0f };

int main(void) {

	int exitcode = 0;

	glfwInitHint(GLFW_WAYLAND_LIBDECOR, GLFW_WAYLAND_DISABLE_LIBDECOR);

	if (!glfwInit()) {
		fprintf(stderr, "Failed to initialize GLFW\n");
		exitcode = 1;
		goto cleanup;
	}

	printf("GLFW %s\n", glfwGetVersionString());

	glfwSetErrorCallback(glfwErrorCallback);

	glfwWindowHint(GLFW_RESIZABLE, GLFW_FALSE);

	GLFWwindow* window = glfwCreateWindow(1024, 1024, "Verlet Integration", NULL, NULL);
	if (!window) {
		fprintf(stderr, "Failed to create GLFW window\n");
		exitcode = 1;
		goto cleanup;
	}

	glfwSetFramebufferSizeCallback(window, glfwFramebufferSizeCallback);
	glfwSetCursorPosCallback(window, glfwCursorPosCallback);
	glfwSetMouseButtonCallback(window, glfwMouseButtonCallback);

	glfwMakeContextCurrent(window);

	glfwSwapInterval(0); // VSync

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
		fprintf(stderr, "Failed to initialize GLAD\n");
		exitcode = 1;
		goto cleanup;
	}

	printf("OpenGL %s\n", glGetString(GL_VERSION));

	printf("Compiling vertex shader...\n");
	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	{
		const GLchar shaderSource[] = {
			#embed "shader/verlet.vert"
		};
		GLsizei sourceLength = sizeof(shaderSource);
		// printf("Vertex Shader:\n%.*s\n", sourceLength, shaderSource);
		glShaderSource(vertexShader, 1, (const GLchar* []) { shaderSource }, &sourceLength);
		glCompileShader(vertexShader);
		GLint shaderCompiled;
		glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &shaderCompiled);
		if (shaderCompiled != GL_TRUE) {
			GLchar infoLog[MAX_INFO_LOG];
			GLsizei logLength;
			glGetShaderInfoLog(vertexShader, sizeof(infoLog), &logLength, infoLog);
			fprintf(stderr, "Failed to compile vertex shader:\n%.*s\n", logLength, infoLog);
			exitcode = 1;
			goto cleanup;
		}
	}

	printf("Compiling fragment shader...\n");
	GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
	{
		const GLchar shaderSource[] = {
			#embed "shader/verlet.frag"
		};
		GLsizei sourceLength = sizeof(shaderSource);
		// printf("Fragment Shader:\n%.*s\n", sourceLength, shaderSource);
		glShaderSource(fragmentShader, 1, (const GLchar* []) { shaderSource }, &sourceLength);
		glCompileShader(fragmentShader);
		GLint shaderCompiled;
		glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &shaderCompiled);
		if (shaderCompiled != GL_TRUE) {
			GLchar infoLog[MAX_INFO_LOG];
			GLsizei logLength;
			glGetShaderInfoLog(fragmentShader, sizeof(infoLog), &logLength, infoLog);
			fprintf(stderr, "Failed to compile fragment shader:\n%.*s\n", logLength, infoLog);
			exitcode = 1;
			goto cleanup;
		}
	}

	printf("Linking shader program...\n");
	GLuint shaderProgram = glCreateProgram();
	{
		glAttachShader(shaderProgram, vertexShader);
		glAttachShader(shaderProgram, fragmentShader);
		glLinkProgram(shaderProgram);
		GLint programLinked;
		glGetProgramiv(shaderProgram, GL_LINK_STATUS, &programLinked);
		if (programLinked != GL_TRUE) {
			GLchar infoLog[MAX_INFO_LOG];
			GLsizei logLength;
			glGetProgramInfoLog(shaderProgram, sizeof(infoLog), &logLength, infoLog);
			fprintf(stderr, "Failed to link shader program:\n%.*s\n", logLength, infoLog);
			exitcode = 1;
			goto cleanup;
		}
	}

	srand(time(NULL));

	GLuint ssboPos, ssboVel, ssboAcc;
	glGenBuffers(1, &ssboPos);
	glGenBuffers(1, &ssboVel);
	glGenBuffers(1, &ssboAcc);

	float pos0[INSTANCES][2];
	for (int i = 0; i < INSTANCES; i++) {
		pos0[i][0] = (random() - 0.5f) * 2.0f;
		pos0[i][1] = (random() - 0.5f) * 2.0f;
	}
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboPos);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(pos0), pos0, GL_DYNAMIC_DRAW);

	float vel0[INSTANCES][2];
	for (int i = 0; i < INSTANCES; i++) {
		vel0[i][0] = (random() - 0.5f) * 4.0f;
		vel0[i][1] = (random() - 0.5f) * 4.0f;
	}

	glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboVel);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(vel0), vel0, GL_DYNAMIC_DRAW);

	float acc0[INSTANCES][2];
	for (int i = 0; i < INSTANCES; i++) {
		acc0[i][0] = 0.0f;
		acc0[i][1] = 0.0f;
	}
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboAcc);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(acc0), acc0, GL_DYNAMIC_DRAW);

	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssboPos);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ssboVel);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, ssboAcc);

	GLint shaderDeltaTime = glGetUniformLocation(shaderProgram, "uDeltaTime");
	GLint shaderMouse = glGetUniformLocation(shaderProgram, "uMouse");
	
	glfwSetTime(0.0);
	float timePrev = 0.0f;
	float fpsTimer = 0.0f;
	int fpsCounter = 0;

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

		glClearColor(1.0f, 1.0f, 1.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		glUseProgram(shaderProgram);
		glUniform1f(shaderDeltaTime, deltaTime);
		glUniform4fv(shaderMouse, 1, mouse);
		glDrawArraysInstanced(GL_TRIANGLES, 0, 6, INSTANCES);

		glfwSwapBuffers(window);
		glfwPollEvents();
	}

cleanup:
	glfwDestroyWindow(window);
	glfwTerminate();

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
