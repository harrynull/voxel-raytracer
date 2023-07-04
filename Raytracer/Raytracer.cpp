#include <iostream>
#include "Window.h"
#include "Shader.h"


int main() {
	Renderer renderer;
	Window window(800, 600, "Raytracer", renderer);
	glClearColor(0.f, 0.f, 0.f, 1.0f);
	window.mainLoop();
}
