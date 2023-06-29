#pragma once
#include <glad/glad.h>

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

	void setInt(const char* name, int value) const noexcept {
		glUniform1i(glGetUniformLocation(program, name), value);
	}
private:
	unsigned int program;
};
