#pragma once
#include <memory>
#include <vector>
#include <unordered_map>
class SVO {
public:
	SVO() = default;
	void toSVDAG(std::vector<int32_t>& result);
	static SVO voxelise(const char* filename, int size);
	static SVO from3DArray(int size, int* data);
	static SVO* sample();
	size_t hash();

private:
	void toSVDAGImpl(std::vector<int32_t>& result, std::unordered_map<size_t, size_t>& hashToIndex);
	SVO* children[8] = { nullptr };
	size_t hashValue = 0;
};
