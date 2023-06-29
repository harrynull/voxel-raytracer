#include "Renderer.h"

#include <cassert>
#include <iostream>

#include "Window.h"

void error(int i = 0) {
	auto err = glGetError();
	if (err != GL_NO_ERROR) {
		std::cerr << i << "Error: " << err << std::endl;
		exit(1);
	}
}

void Renderer::init() noexcept{
	renderShader.emplace("shaders/vertex.glsl", "shaders/fragment.glsl", nullptr);
	computeShader.emplace(nullptr, nullptr, "shaders/compute.glsl");
	texture.emplace(window->width(), window->height());
}

void Renderer::render() const noexcept
{
	assert(texture.has_value() && computeShader.has_value() && renderShader.has_value() && window);
	texture.value().bind();
	computeShader.value().use();
	glDispatchCompute(window->width(), window->height(), 1);
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

	renderShader.value().use();
	renderShader.value().setInt("tex", 0);
	glActiveTexture(GL_TEXTURE0);

	static unsigned int quadVAO = 0, quadVBO;
	texture.value().bind();
	if (quadVAO == 0) {
		float quadVertices[] = {
			// positions        // texture Coords
			-1.0f,  1.0f, 0.0f, 0.0f, 1.0f,
			-1.0f, -1.0f, 0.0f, 0.0f, 0.0f,
			 1.0f,  1.0f, 0.0f, 1.0f, 1.0f,
			 1.0f, -1.0f, 0.0f, 1.0f, 0.0f,
		};
		// setup plane VAO
		glGenVertexArrays(1, &quadVAO);
		glGenBuffers(1, &quadVBO);
		glBindVertexArray(quadVAO);
		glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
		glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), &quadVertices, GL_STATIC_DRAW);
		glEnableVertexAttribArray(0);
		glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
		glEnableVertexAttribArray(1);
		glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
	}
	glClear(GL_COLOR_BUFFER_BIT);
	glBindVertexArray(quadVAO);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	glBindVertexArray(0);
}


