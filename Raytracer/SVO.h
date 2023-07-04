#pragma once
#include <memory>
#include <vector>
#include <unordered_map>
#include <glm/glm.hpp>
class SVO {
public:
	SVO(size_t size) : size(size) {}
	SVO(SVO&) = delete;
	SVO(SVO&&) = delete;
	~SVO() { for (auto& child : children) delete child; }
	SVO& operator=(SVO&) = delete;
	SVO& operator=(SVO&&) = delete;

	void set(size_t x, size_t y, size_t z, glm::uvec3 rgb);
	void toSVDAG(std::vector<int32_t>& result);
	static SVO voxelise(const char* filename, int size);
	static SVO from3DArray(int size, int* data);
	static SVO* sample();
	size_t hash();

private:
	void toSVDAGImpl(std::vector<int32_t>& result, std::unordered_map<size_t, size_t>& hashToIndex);
	SVO* children[8] = { nullptr };
	size_t hashValue = 0;
	size_t size = 0;
};
