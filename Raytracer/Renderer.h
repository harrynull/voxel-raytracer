#pragma once
#include <optional>
#include "Shader.h"
#include "Texture.h"
#include <glm/glm.hpp>

class Window;

class Renderer {
public:
	Renderer() = default;
	~Renderer() = default;

	void init() noexcept;
	void render() const noexcept;
	void update() noexcept;
	void setWindow(Window* window) noexcept { this->window = window; }
	void handleKey(char key, int action) noexcept;
	void handleMouseMove(double xpos, double ypos) noexcept;
	void handleMouse(int button, int action, double xpos, double ypos) noexcept;
private:
	std::optional<Shader> computeShader = std::nullopt, renderShader = std::nullopt;
	std::optional<Texture> texture = std::nullopt;
	Window* window;
	GLuint quadVAO = 0, quadVBO = 0; // for rendering the image (screen quad)
	GLuint svdagBuffer = 0;
	glm::vec3 cameraPos = { -2.0f, -2.0f, -2.0f };
	glm::vec3 cameraUp = { 0.0f, 1.0f, 0.0f };
	glm::vec3 cameraFront = { 0.0f, 0.0f, 1.0f };

	bool mouseClicked = false;
	glm::vec2 lastMousePos = { 0.0f, 0.0f };
	bool keyPressed[256] = { false };
};
