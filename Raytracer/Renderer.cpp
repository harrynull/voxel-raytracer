#include "Renderer.h"

#include <cassert>
#include <iostream>

#include "Window.h"
#include "SVO.h"
#include <algorithm>
#include <math.h>

void error(int i = 0) {
	auto err = glGetError();
	if (err != GL_NO_ERROR) {
		std::cerr << i << "Error: " << err << std::endl;
		exit(1);
	}
}

void Renderer::init() noexcept {
	renderShader.emplace("shaders/vertex.glsl", "shaders/fragment.glsl", nullptr);
	computeShader.emplace(nullptr, nullptr, "shaders/compute.glsl");
	computeShader->use();
	texture.emplace(window->width(), window->height());

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

	texture.value().bind();
	glBindVertexArray(quadVAO);

	auto svo = SVO::sample();
	std::vector<int32_t> svdag;
	svo->toSVDAG(svdag);

	glCreateBuffers(1, &svdagBuffer);
	glNamedBufferStorage(svdagBuffer, svdag.size() * sizeof(int32_t), svdag.data(), 0);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, svdagBuffer);
}

void Renderer::render() const noexcept {
	// Raytrace with compute shader
	computeShader->use();
	computeShader->setVec3("cameraPos", cameraPos);
	computeShader->setVec3("cameraUp", cameraUp);
	computeShader->setVec3("cameraFront", cameraFront);

	glDispatchCompute(window->width(), window->height(), 1);
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

	// Render the result of the compute shader
	renderShader->use();
	renderShader->setInt("tex", 0);
	glActiveTexture(GL_TEXTURE0);
	glClear(GL_COLOR_BUFFER_BIT);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

using namespace glm;
static vec3 rotate(float theta, const vec3& v, const vec3& w) {
	const float c = cosf(theta);
	const float s = sinf(theta);

	const vec3 v0 = dot(v, w) * w;
	const vec3 v1 = v - v0;
	const vec3 v2 = cross(w, v1);

	return v0 + c * v1 + s * v2;
}
void Renderer::handleKey(char key) noexcept {
	constexpr float PI = 3.14159265358979323846;
	constexpr float speed = 1.f;
	switch (key) {
	case 'W':
		cameraPos += cameraFront * speed;
		break;
	case 'S':
		cameraPos -= cameraFront * speed;
		break;
	case 'A':
		cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * speed;
		break;
	case 'D':
		cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * speed;
		break;
	case ' ':
		cameraPos += cameraUp * speed;
		break;
	case 'Z':
		cameraPos -= cameraUp * speed;
		break;
	case 'Q': //left
		cameraFront = rotate(PI / 180.f, cameraFront, cameraUp);
		break;
	case 'E':// right
		cameraFront = rotate(-PI / 180.f, cameraFront, cameraUp);
		break;
	case 'R': // up
		cameraFront = rotate(PI / 180.f, cameraFront, glm::normalize(glm::cross(cameraFront, cameraUp)));
		break;
	case 'F': // down
		cameraFront = rotate(-PI / 180.f, cameraFront, glm::normalize(glm::cross(cameraFront, cameraUp)));
		break;
	}
	printf("Camera position: (%f, %f, %f)\n", cameraPos.x, cameraPos.y, cameraPos.z);
	printf("CameraFront: (%f, %f, %f)\n", cameraFront.x, cameraFront.y, cameraFront.z);
}

