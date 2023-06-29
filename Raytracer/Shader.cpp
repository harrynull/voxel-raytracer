#include "Shader.h"

#include <fstream>
#include <iostream>
#include <string>

static int load(const char* filePath, unsigned int type) {
	if (!filePath) return -1;

	auto shader = glCreateShader(type);
	std::ifstream ifs(filePath);
	if (!ifs) {
		std::cerr << "Shader file not found: " << filePath << std::endl;
		exit(1);
	}

	std::string content((std::istreambuf_iterator<char>(ifs)),
		(std::istreambuf_iterator<char>()));
	auto cstr = content.c_str();
	glShaderSource(shader, 1, &cstr, nullptr);
	glCompileShader(shader);

	// check for errors
	int success;
	char infoLog[512];
	glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
	if (!success) {
		glGetShaderInfoLog(shader, 512, NULL, infoLog);
		std::cerr << "Shader compilation failure:" << infoLog << std::endl;
		exit(1);
	}
	return shader;
}

Shader::Shader(const char* vertexPath, const char* fragmentPath, const char* computePath) {
	auto vertexShader = load(vertexPath, GL_VERTEX_SHADER);
	auto fragmentShader = load(fragmentPath, GL_FRAGMENT_SHADER);
	auto computeShader = load(computePath, GL_COMPUTE_SHADER);

	program = glCreateProgram();
	if (vertexShader != -1) glAttachShader(program, vertexShader);
	if (fragmentShader != -1) glAttachShader(program, fragmentShader);
	if (computeShader != -1) glAttachShader(program, computeShader);
	glLinkProgram(program);
	GLint success;
	// check for linking errors
	glGetProgramiv(program, GL_LINK_STATUS, &success);
	char infoLog[512];
	if (!success) {
		glGetProgramInfoLog(program, 512, NULL, infoLog);
		std::cout << "Linking error: " << infoLog << std::endl;
		exit(1);
	}
	if (vertexShader != -1) glDeleteShader(vertexShader);
	if (fragmentShader != -1) glDeleteShader(fragmentShader);
	if (computeShader != -1) glDeleteShader(computeShader);
}