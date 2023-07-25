#define _CRT_SECURE_NO_WARNINGS

#include "Renderer.h"

#include <cassert>
#include <iostream>

#include "Window.h"
#include "SVO.h"
#include <algorithm>

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_opengl3.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

#include <sstream>

#define _USE_MATH_DEFINES
#include <math.h>

#include <filesystem>

void error(int i = 0) {
	auto err = glGetError();
	if (err != GL_NO_ERROR) {
		std::cerr << i << "Error: " << err << std::endl;
		exit(1);
	}
}

void Renderer::loadSVO(SVO& svo) {
	std::cout << "Scene loaded / generated!" << std::endl;
	std::vector<int32_t> svdag;
	std::vector<SVO::Material> materials;
	svo.toSVDAG(svdag, materials);
	std::cout << "SVDAG size " << svdag.size()
		<< " with " << materials.size() << " materials" << std::endl;
	sceneSize = svdag.size();
	nMaterials = materials.size();
	rootSize = svo.getSize();

	if(svdagBuffer) glDeleteBuffers(1, &svdagBuffer);
	if(materialsBuffer) glDeleteBuffers(1, &materialsBuffer);

	glCreateBuffers(1, &svdagBuffer);
	glNamedBufferStorage(svdagBuffer, svdag.size() * sizeof(int32_t), svdag.data(), 0);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 1, svdagBuffer);

	glCreateBuffers(1, &materialsBuffer);
	glNamedBufferStorage(materialsBuffer, materials.size() * sizeof(SVO::Material), materials.data(), 0);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 2, materialsBuffer);

	computeShader->use();
	computeShader->setInt("RootSize", svo.getSize());
	currentFrameCount = 0;
}

void Renderer::loadScenes() {
	scenes.clear();
	scenes.push_back(std::make_unique<TestScene>());
	scenes.push_back(std::make_unique<TerrainScene>());
	scenes.push_back(std::make_unique<StairScene>());
	// iter vox files
	if (std::filesystem::exists("vox")) {
		for (auto& p : std::filesystem::recursive_directory_iterator("vox")) {
			if (p.path().extension() == ".vox") {
				scenes.push_back(std::make_unique<VoxModelScene>(p.path().string()));
			}
		}
	}
}

void Renderer::init() noexcept {
	renderShader.emplace("shaders/vertex.glsl", "shaders/fragment.glsl", nullptr);
	computeShader.emplace(nullptr, nullptr, "shaders/compute.glsl");
	computeShader->use();
	texture.emplace(window->width(), window->height());

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls

	ImGui_ImplGlfw_InitForOpenGL(window->getGLFWwindow(), true);
	ImGui_ImplOpenGL3_Init();

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

	computeShader->use();
	// autoFocus buffer for receving output
	glCreateBuffers(1, &autoFocusBuffer);
	glNamedBufferStorage(autoFocusBuffer, sizeof(float), nullptr, GL_MAP_READ_BIT);
	glBindBufferBase(GL_SHADER_STORAGE_BUFFER, 3, autoFocusBuffer);
	// map to autoFocus
	autoFocus = static_cast<float*>(glMapNamedBufferRange(autoFocusBuffer, 0, sizeof(float), GL_MAP_READ_BIT));

	loadScenes();
	loadSVO(*(scenes[0]->load(0))); // load the first scene
}

void Renderer::renderUI() noexcept {
	ImGui_ImplOpenGL3_NewFrame();
	ImGui_ImplGlfw_NewFrame();
	ImGui::NewFrame();
	ImGui::Begin("SVO Raytracer");

	ImGuiIO& io = ImGui::GetIO(); (void)io;
	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / io.Framerate, io.Framerate);
	ImGui::Text("Frames accumulated %d", currentFrameCount);
	ImGui::Text("Scene size: %d bytes, materials count: %d, root size: %d", sceneSize, nMaterials, rootSize);
	ImGui::Spacing();
	ImGui::DragFloat3("Camera Position", &cameraPos[0]);
	ImGui::DragFloat3("Camera front", &cameraFront[0]);
	ImGui::Spacing();
	ImGui::ColorEdit3("Sky color", &skyColor[0]);
	ImGui::ColorEdit3("Sun color", &sunColor[0]);
	ImGui::DragFloat3("Sun direction", &sunDir[0]);
	ImGui::Spacing();
	ImGui::Checkbox("Enable Depth of Field", &enableDepthOfField);
	ImGui::DragFloat("Focal Length", &focalLength);
	ImGui::DragFloat("Lens Radius", &lenRadius);
	ImGui::Text("Look-at distance: %.3f", *autoFocus);
	ImGui::SameLine();
	if (ImGui::Button("Auto focus")) {
		focalLength = *autoFocus;
	}
	ImGui::Spacing();
	ImGui::Checkbox("Fast Mode", &fastMode);
	ImGui::Spacing();


	static int currentSelection = 0;
	auto& currentScene = scenes[currentSelection];
	if (ImGui::BeginCombo("Scene", currentScene->getDisplayName(), 0))
	{
		for (size_t n = 0; n < scenes.size(); n++)
		{
			const bool isSelected = (currentSelection == n);
			if (ImGui::Selectable(scenes[n]->getDisplayName(), isSelected)) {
				currentSelection = n;
			    loadSVO(*(scenes[n]->load(32)));
				scenes[n]->release();
			}
			if (isSelected)
				ImGui::SetItemDefaultFocus();
		}
		ImGui::EndCombo();
	}

	static char paramInput[64] = "32";
	if (currentScene->hasParam()) {
		ImGui::InputText(currentScene->getParamName(), paramInput, 64, ImGuiInputTextFlags_CharsDecimal);
		ImGui::SameLine();
		if (ImGui::Button("Set")) {
			loadSVO(*(currentScene->load(std::stoi(paramInput))));
		}
	}

	if (ImGui::Button("Screenshot")) {
		takeScreenshot();
	}
	ImGui::SameLine();
	if (ImGui::Button("Re-render")) {
		currentFrameCount = 0;
	}

	ImGui::End();
}

void Renderer::checkForAccumulationFrameInvalidation() noexcept {
	static glm::vec3 cameraPosLastFrame = cameraPos, cameraFrontLastFrame = cameraFront;
	static bool enableDepthOfFieldLastFrame = enableDepthOfField;
	static float focalLengthLastFrame = focalLength, lenRadiusLastFrame = lenRadius;
	static glm::vec3 skyColorLastFrame = skyColor, sunColorLastFrame = sunColor, sunDirLastFrame = sunDir;
	static bool fastModeLastFrame = fastMode;
	if (cameraPos != cameraPosLastFrame ||
		cameraFront != cameraFrontLastFrame ||
		enableDepthOfField != enableDepthOfFieldLastFrame ||
		focalLength != focalLengthLastFrame ||
		lenRadius != lenRadiusLastFrame ||
		skyColor != skyColorLastFrame ||
		sunColor != sunColorLastFrame ||
		sunDir != sunDirLastFrame ||
		fastMode != fastModeLastFrame
		)  currentFrameCount = 0;
	cameraPosLastFrame = cameraPos;
	cameraFrontLastFrame = cameraFront;
	enableDepthOfFieldLastFrame = enableDepthOfField;
	focalLengthLastFrame = focalLength;
	lenRadiusLastFrame = lenRadius;
	skyColorLastFrame = skyColor;
	sunColorLastFrame = sunColor;
	sunDirLastFrame = sunDir;
	fastModeLastFrame = fastMode;
}

void Renderer::render() noexcept {
	renderUI();
	checkForAccumulationFrameInvalidation();

	// Raytrace with compute shader
	computeShader->use();
	computeShader->setVec3("CameraPos", cameraPos);
	computeShader->setVec3("CameraUp", cameraUp);
	computeShader->setVec3("CameraFront", cameraFront);
	computeShader->setVec3("RandomSeed", glm::vec3(rand(), rand(), rand()));
	computeShader->setInt("CurrentFrameCount", currentFrameCount);
	computeShader->setBool("DepthOfField", enableDepthOfField);
	computeShader->setFloat("FocalLength", focalLength);
	computeShader->setFloat("LenRadius", lenRadius);
	computeShader->setVec3("SkyColor", skyColor);
	computeShader->setVec3("SunColor", sunColor);
	computeShader->setVec3("SunDir", sunDir);
	computeShader->setBool("FastMode", fastMode);

	currentFrameCount += 1;

	glDispatchCompute(window->width(), window->height(), 1);
	glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

	// Render the result of the compute shader
	renderShader->use();
	renderShader->setInt("tex", 0);
	glActiveTexture(GL_TEXTURE0);
	glClear(GL_COLOR_BUFFER_BIT);
	glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
	
	ImGui::Render();
	ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
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
	if (keyPressed['W']) cameraPos += cameraFront * speed;
	if (keyPressed['S']) cameraPos -= cameraFront * speed;
	if (keyPressed['A']) cameraPos -= glm::normalize(glm::cross(cameraFront, cameraUp)) * speed;
	if (keyPressed['D']) cameraPos += glm::normalize(glm::cross(cameraFront, cameraUp)) * speed;
	if (keyPressed[' ']) cameraPos += cameraUp * speed;
	if (keyPressed['Z']) cameraPos -= cameraUp * speed;
	if (keyPressed['O']) focalLength += 1.f;
	if (keyPressed['P']) focalLength = std::max(focalLength - 1.f, 0.0f);
	if (keyPressed['I']) focalLength = *autoFocus;
	if (keyPressed['K']) lenRadius += 0.01f;
	if (keyPressed['L']) lenRadius = std::max(lenRadius - 0.01f, 0.0f);
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
		takeScreenshot();
	}
}

void Renderer::takeScreenshot() {
	std::stringstream filename;
	filename << "screenshots/screenshot_" << time(nullptr) << "_" << currentFrameCount << ".png";
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