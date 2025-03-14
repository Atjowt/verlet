#include <stdio.h>
#include <stdlib.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#define MAX_INFO_LOG 512

void glfwErrorCallback(int code, const char* desc);
void glfwFramebufferSizeCallback(GLFWwindow* window, int width, int height);

int main(void) {

	int exitcode = 0;

	glfwInitHint(GLFW_WAYLAND_DISABLE_LIBDECOR, GLFW_TRUE);

	if (!glfwInit()) {
		fprintf(stderr, "Failed to initialize GLFW\n");
		exitcode = 1;
		goto cleanup;
	} else {
		printf("GLFW: %s\n", glfwGetVersionString());
	}

	glfwSetErrorCallback(glfwErrorCallback);

	GLFWwindow* window = glfwCreateWindow(512, 512, "Verlet Integration", NULL, NULL);
	if (!window) {
		fprintf(stderr, "Failed to create GLFW window\n");
		exitcode = 1;
		goto cleanup;
	} else {
		int width, height;
		glfwGetWindowSize(window, &width, &height);
		printf("Window: %dx%d\n", width, height);
	}

	glfwSetFramebufferSizeCallback(window, glfwFramebufferSizeCallback);

	glfwMakeContextCurrent(window);
	{
		int major = glfwGetWindowAttrib(window, GLFW_CONTEXT_VERSION_MAJOR);
		int minor = glfwGetWindowAttrib(window, GLFW_CONTEXT_VERSION_MINOR);
		printf("Context: %d.%d\n", major, minor);
	}

	glfwSwapInterval(1); // VSync

	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
		fprintf(stderr, "Failed to initialize GLAD\n");
		exitcode = 1;
		goto cleanup;
	} else {
		const GLubyte* version = glGetString(GL_VERSION);
		printf("OpenGL: %s\n", version);
	}

	printf("Compiling vertex shader...\n");
	GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
	{
		const GLchar shaderSource[] = {
			#embed "shader/main.vert"
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
			#embed "shader/main.frag"
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


#define INSTANCES 4

	GLuint ssboPos, ssboVel, ssboAcc;
	glGenBuffers(1, &ssboPos);
	glGenBuffers(1, &ssboVel);
	glGenBuffers(1, &ssboAcc);

	float pos0[INSTANCES][2] = {
		{  0.0f, 0.0f, },
		{  0.1f, 0.1f, },
		{  0.2f, 0.2f, },
		{  0.3f, 0.3f, },
	};
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboPos);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(pos0), pos0, GL_DYNAMIC_DRAW);

	float vel0[INSTANCES][2] = {
		{  0.0f, 0.0f, },
		{  0.1f, 0.1f, },
		{  0.2f, 0.2f, },
		{  0.3f, 0.3f, },
	};
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboVel);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(vel0), vel0, GL_DYNAMIC_DRAW);

	float acc0[INSTANCES][2] = {
		{  0.0f, 0.0f, },
		{  0.0f, 0.0f, },
		{  0.0f, 0.0f, },
		{  0.0f, 0.0f, },
	};
	glBindBuffer(GL_SHADER_STORAGE_BUFFER, ssboAcc);
	glBufferData(GL_SHADER_STORAGE_BUFFER, sizeof(acc0), acc0, GL_DYNAMIC_DRAW);

	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 0, ssboPos);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, ssboVel);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, ssboAcc);

	GLint shaderDeltaTime = glGetUniformLocation(shaderProgram, "uDeltaTime");
	
	glfwSetTime(0.0);
	double timePrev = 0.0;
	while (!glfwWindowShouldClose(window)) {
		double timeCurr = glfwGetTime();
		double deltaTime = timeCurr - timePrev;
		printf("%.2f FPS\n", 1.0f / deltaTime);
		timePrev = timeCurr;
		glClearColor(0.5f, 0.5f, 1.0f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT);
		glUseProgram(shaderProgram);
		glUniform1f(shaderDeltaTime, deltaTime);
		glDrawArraysInstanced(GL_TRIANGLES, 0, 6, 4);
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

void glfwFramebufferSizeCallback(GLFWwindow* window, int width, int height) {
	glViewport(0, 0, width, height);
}
