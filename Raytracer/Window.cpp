#include "Window.h"

#include <glad/glad.h>
#include <functional>

static void initGlfw() {
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
}

Window::Window(size_t width, size_t height, const char* title, Renderer& renderer) :
	renderer(renderer), mWidth(width), mHeight(height) {

	initGlfw();
	window = glfwCreateWindow(width, height, title, nullptr, nullptr);
	glfwMakeContextCurrent(window);
	gladLoadGL();
	glViewport(0, 0, width, height);
	renderer.setWindow(this);

	static Renderer& r = renderer;
	glfwSetKeyCallback(window, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
		r.handleKey(
			key == 340 ? 'Z' : key, // remap left shift to Z
			action
		);
		});
	glfwSetCursorPosCallback(window, [](GLFWwindow* window, double xpos, double ypos) {
		r.handleMouseMove(xpos, ypos);
		});
	glfwSetMouseButtonCallback(window, [](GLFWwindow* window, int button, int action, int mods) {
		double x, y;
		glfwGetCursorPos(window, &x, &y);
		r.handleMouse(button, action, x, y);
		});
}

Window::~Window() {
	glfwDestroyWindow(window);
	glfwTerminate();
}

void Window::swapBuffers() const noexcept {
	glfwSwapBuffers(window);
}

void Window::mainLoop() {
	double lastTime = glfwGetTime();
	int nFrames = 0;
	renderer.init();
	glfwSwapInterval(0);
	while (!glfwWindowShouldClose(window)) {
		glfwPollEvents();

		renderer.update();
		renderer.render();
		swapBuffers();

		double currentTime = glfwGetTime();
		nFrames++;
		if (currentTime - lastTime >= 1.0) {
			printf("%d fps\n", int(nFrames));
			nFrames = 0;
			lastTime = currentTime;
		}
	}
}