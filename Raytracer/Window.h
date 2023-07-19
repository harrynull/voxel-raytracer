#pragma once

#include<glad/glad.h>
#include<GLFW/glfw3.h>
#include "Renderer.h"

class Window {
public:
	Window(size_t width, size_t height, const char* title, Renderer& renderer);
	~Window();

	Window(Window&&) = delete;
	Window(const Window&) = delete;
	Window& operator=(Window&&) = delete;
	Window& operator=(const Window&) = delete;

	void swapBuffers() const noexcept;
	void mainLoop();
	size_t width() const noexcept { return mWidth; }
	size_t height() const noexcept { return mHeight; }

	GLFWwindow* getGLFWwindow() const noexcept { return window; }
private:
	GLFWwindow* window;
	Renderer& renderer;
	size_t mWidth, mHeight;
};
