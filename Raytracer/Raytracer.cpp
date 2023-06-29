#include <iostream>
#include "Window.h"
#include "Shader.h"


int main() {
	Renderer renderer;
	Window window(800, 600, "Raytracer", renderer);
	glClearColor(0.07f, 0.13f, 0.17f, 1.0f);
	window.mainLoop();
}
