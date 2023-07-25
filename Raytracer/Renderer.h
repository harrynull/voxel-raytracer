#pragma once
#include <optional>
#include <memory>
#include <glm/glm.hpp>
#include "Shader.h"
#include "Texture.h"
#include "Scene.h"

class Window;

class Renderer {
public:
	Renderer() = default;
	~Renderer() = default;

	void init() noexcept;
	void render() noexcept;
	void update() noexcept;
	void setWindow(Window* window) noexcept { this->window = window; }
	void handleKey(char key, int action) noexcept;
	void handleMouseMove(double xpos, double ypos) noexcept;
	void handleMouse(int button, int action, double xpos, double ypos) noexcept;
private:
	void renderUI() noexcept;
	void takeScreenshot();
	void checkForAccumulationFrameInvalidation() noexcept;
	void loadSVO(SVO& svo);
	void loadScenes();

	std::optional<Shader> computeShader = std::nullopt, renderShader = std::nullopt;
	std::optional<Texture> texture = std::nullopt;
	Window* window;
	GLuint quadVAO = 0, quadVBO = 0; // for rendering the image (screen quad)
	GLuint svdagBuffer = 0, materialsBuffer, autoFocusBuffer;
	glm::vec3 cameraPos = { -2.6f, 0.7f, -0.5f };
	glm::vec3 cameraUp = { 0.0f, 1.0f, 0.0f };
	glm::vec3 cameraFront = { 0.7f, -0.2f, 0.7f };

	bool mouseClicked = false;
	glm::vec2 lastMousePos = { 0.0f, 0.0f };
	bool keyPressed[256] = { false };

	size_t currentFrameCount = 0;
	bool enableDepthOfField = false;
	float focalLength = 5.f;
	float lenRadius = 0.1f;

	bool fastMode = false;

	glm::vec3 sunDir { -0.5, 0.75, 0.8 };
	glm::vec3 sunColor { 1, 1, 1 };
	glm::vec3 skyColor { .53, .81, .92 };

	float* autoFocus = nullptr;

	// stats
	size_t sceneSize = 0, nMaterials = 0, rootSize = 0;

	// scenes
	std::vector<std::unique_ptr<Scene>> scenes;
};
