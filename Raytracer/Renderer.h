#pragma once
#include <optional>
#include "Shader.h"
#include "Texture.h"

class Window;

class Renderer {
public:
	Renderer() {}
	~Renderer() = default;

	void init() noexcept;
	void render() const noexcept;
	void setWindow(Window* window) noexcept { this->window = window; }
private:
	std::optional<Shader> computeShader = std::nullopt, renderShader = std::nullopt;
	std::optional<Texture> texture = std::nullopt;
	Window* window;
};
