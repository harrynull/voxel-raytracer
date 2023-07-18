#define _CRT_SECURE_NO_WARNINGS

#include "Renderer.h"

#include <cassert>
#include <iostream>

#include "Window.h"
#include "SVO.h"
#include <algorithm>

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <sstream>

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

	//auto svo = SVO::fromVox("vox/teapot.vox");
	auto svo = SVO::terrain(1024);
	//auto svo = SVO::sample();
	std::cout << "Scene loaded / generated!" << std::endl;
	std::vector<int32_t> svdag;
	std::vector<SVO::Material> materials;
	svo->toSVDAG(svdag, materials);
	std::cout << "SVDAG size " << svdag.size()
		<< " with " << materials.size() << " materials" << std::endl;

	glCreateBuffers(1, &svdagBuffer);
	glNamedBufferStorage(svdagBuffer, svdag.size() * sizeof(int32_t), svdag.data(), 0);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, svdagBuffer);

	glCreateBuffers(1, &materialsBuffer);
	glNamedBufferStorage(materialsBuffer, materials.size() * sizeof(SVO::Material), materials.data(), 0);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, materialsBuffer);

	glCreateBuffers(1, &autoFocusBuffer);
	glNamedBufferStorage(autoFocusBuffer, sizeof(float), nullptr, GL_MAP_READ_BIT);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, autoFocusBuffer);
	// map to autoFocus
	autoFocus = static_cast<float*>(glMapNamedBufferRange(autoFocusBuffer, 0, sizeof(float), GL_MAP_READ_BIT));

	computeShader->use();
	computeShader->setInt("RootSize", svo->getSize());

	delete svo;
}

void Renderer::render() noexcept {
	// Raytrace with compute shader
	computeShader->use();
	computeShader->setVec3("cameraPos", cameraPos);
	computeShader->setVec3("cameraUp", cameraUp);
	computeShader->setVec3("cameraFront", cameraFront);
	computeShader->setVec3("RandomSeed", glm::vec3(rand(), rand(), rand()));
	computeShader->setInt("currentFrameCount", currentFrameCount);
	computeShader->setBool("DepthOfField", enableDepthOfField);
	computeShader->setFloat("FocalLength", focalLength);
	computeShader->setFloat("LenRadius", lenRadius);

	currentFrameCount += 1;

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
	float speed = keyPressed['X'] ? 1.f : 0.1f;
	if (keyPressed['C']) speed *= 100;
	auto oldCameraPos = cameraPos;
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
	if (keyPressed['O']) {
		focalLength += 1.f;
		currentFrameCount = 0;
	}
	if (keyPressed['P']) {
		focalLength -= 1.f;
		focalLength = std::max(focalLength, 0.0f);
		currentFrameCount = 0;
	}
	if (keyPressed['I']) {
		focalLength = *autoFocus;
		currentFrameCount = 0;
	}
	if (keyPressed['K']) {
		lenRadius += 0.01f;
		currentFrameCount = 0;
	}
	if (keyPressed['L']) {
		lenRadius -= 0.01f;
		lenRadius = std::max(lenRadius, 0.0f);
		currentFrameCount = 0;
	}
	if (cameraPos != oldCameraPos) currentFrameCount = 0;
}


void Renderer::handleKey(char key, int action) noexcept {
	keyPressed[key] = action != GLFW_RELEASE;

	if (key == 'M' && action == GLFW_PRESS) {
		printf("Camera position: (%f, %f, %f)\n", cameraPos.x, cameraPos.y, cameraPos.z);
		printf("CameraFront: (%f, %f, %f)\n", cameraFront.x, cameraFront.y, cameraFront.z);
		printf("DOF (%d): focalLength = %f, lenRadius = %f, autoFocus=%f\n", enableDepthOfField, focalLength, lenRadius, *autoFocus);
	}
	if (key == 'Q' && action == GLFW_PRESS) {
		enableDepthOfField = !enableDepthOfField;
		printf("Depth of field %s\n", enableDepthOfField ? "enabled" : "disabled");
		currentFrameCount = 0;
	}
	if (key == 'E' && action == GLFW_PRESS) { // take a screenshot
		std::stringstream filename;
		filename << "screenshots/screenshot_" << time(nullptr) << ".png";
		printf("Saving screenshot to %s\n", filename.str().data());
		auto data = texture->dump();
		// convert to int and also flip and apply gamma correction
		constexpr float Gamma = 2.2f;
		std::vector<char> idata(data.size());
		for (size_t x = 0; x < window->width(); x++)
			for (size_t y = 0; y < window->height(); y++)
				for (size_t c = 0; c < 4; c++) {
					float floatVal = data[(x + (window->height() - y - 1) * window->width()) * 4 + c];
					floatVal = clamp(floatVal, 0.f, 1.f);
					floatVal = powf(floatVal, 1.f / Gamma);
					idata[(x + y * window->width()) * 4 + c] =
						int(floatVal * 255);
				}
		stbi_write_png(filename.str().data(), window->width(), window->height(), 4, idata.data(), window->width() * 4);
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
	currentFrameCount = 0;
}

void Renderer::handleMouse(int button, int action, double xpos, double ypos) noexcept {
	mouseClicked = action == GLFW_PRESS;
	lastMousePos = { xpos, ypos };
}