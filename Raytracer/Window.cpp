#include "Window.h"

#include <glad/glad.h>

static void initGlfw() {
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
}

Window::Window(size_t width, size_t height, const char* title, Renderer& renderer):
	renderer(renderer), mWidth(width), mHeight(height) {

	initGlfw();
	window = glfwCreateWindow(width, height, title, nullptr, nullptr);
	glfwMakeContextCurrent(window);
	gladLoadGL();
	glViewport(0, 0, width, height);
	renderer.setWindow(this);
}

Window::~Window() {
	glfwDestroyWindow(window);
	glfwTerminate();
}

void Window::swapBuffers() const noexcept {
	glfwSwapBuffers(window);
}

void Window::mainLoop() {
	renderer.init();
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		renderer.render();
		swapBuffers();
	}
}