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
	void setWindow(Window* window) noexcept { this->window = window; }
	void handleKey(char key) noexcept;
private:
	std::optional<Shader> computeShader = std::nullopt, renderShader = std::nullopt;
	std::optional<Texture> texture = std::nullopt;
	Window* window;
	GLuint quadVAO = 0, quadVBO = 0; // for rendering the image (screen quad)
	GLuint svdagBuffer = 0;
	glm::vec3 cameraPos = { 1.0f, 2.0f, 5.0f };
	glm::vec3 cameraUp = { 0.0f, 1.0f, 0.0f };
	glm::vec3 cameraFront = { 0.0f, 0.0f, -1.0f };
};
