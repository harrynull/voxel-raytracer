#pragma once
#include <glad/glad.h>
#include <glm/glm.hpp>

class Shader {
public:
	Shader(const char* vertexPath, const char* fragmentPath, const char* computePath);
	~Shader() = default;
	Shader(Shader&&) = delete;
	Shader(const Shader&) = delete;
	Shader& operator=(Shader&&) = delete;
	Shader& operator=(const Shader&) = delete;
	void use() const noexcept {
		glUseProgram(program);
	}

	void setBool(const char* name, bool value) const noexcept {
		setInt(name, value);
	}
	void setInt(const char* name, int value) const noexcept {
		glUniform1i(glGetUniformLocation(program, name), value);
	}
	void setFloat(const char* name, float value) const noexcept {
		glUniform1f(glGetUniformLocation(program, name), value);
	}
	void setVec3(const char* name, glm::vec3 value) const noexcept {
		glUniform3f(glGetUniformLocation(program, name), value.x, value.y, value.z);
	}
	void setVec2(const char* name, glm::vec2 value) const noexcept {
		glUniform2f(glGetUniformLocation(program, name), value.x, value.y);
	}
private:
	unsigned int program;
};
