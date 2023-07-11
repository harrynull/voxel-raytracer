#pragma once

#include <functional>
#include <glm/vec3.hpp>

extern void loadVox(const char* filename, std::function<void(int x, int y, int z, glm::uvec3 color)> emit);
