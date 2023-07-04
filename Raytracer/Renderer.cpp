#include "Renderer.h"

#include <cassert>
#include <iostream>

#include "Window.h"
#include "SVO.h"
#include <algorithm>

#define _USE_MATH_DEFINES
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
	std::cout << "Scene loaded / generated!" << std::endl;
	std::vector<int32_t> svdag;
	svo->toSVDAG(svdag);
	std::cout << "SVDAG size " << svdag.size() << std::endl;

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

void Renderer::update() noexcept {
	float speed = keyPressed['X'] ? 0.1 : 0.01;
	if (keyPressed['W'])
		cameraPos += cameraFront * speed;
	if (keyPressed['S'])
		cameraPos -= cameraFront * speed;
	if (keyPressed['A'])
		cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * speed;
	if (keyPressed['D'])
		cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * speed;
	if (keyPressed[' '])
		cameraPos += cameraUp * speed;
	if (keyPressed['Z'])
		cameraPos -= cameraUp * speed;
}


void Renderer::handleKey(char key, int action) noexcept {
	keyPressed[key] = action != GLFW_RELEASE;

	if (key == 'P' && action == GLFW_PRESS) {
		printf("Camera position: (%f, %f, %f)\n", cameraPos.x, cameraPos.y, cameraPos.z);
		printf("CameraFront: (%f, %f, %f)\n", cameraFront.x, cameraFront.y, cameraFront.z);
	}
}


void Renderer::handleMouseMove(double xpos, double ypos) noexcept {
	if (!mouseClicked) return;
	const float sensitivity = 0.01f;
	const float xoffset = lastMousePos.x - xpos;
	const float yoffset = lastMousePos.y - ypos;
	lastMousePos = { xpos, ypos };
	const float yaw = xoffset * sensitivity;
	const float pitch = yoffset * sensitivity;
	cameraFront = rotate(pitch, cameraFront, glm::normalize(glm::cross(cameraFront, cameraUp)));
	cameraFront = rotate(yaw, cameraFront, cameraUp);
}

void Renderer::handleMouse(int button, int action, double xpos, double ypos) noexcept {
	mouseClicked = action == GLFW_PRESS;
	lastMousePos = { xpos, ypos };
}