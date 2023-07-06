#include "SVO.h"
#include <functional>

#include "Terrain.h"

SVO* SVO::sample() {
	SVO* root = new SVO(4);
	root->children[0] = new SVO(2);
	root->children[7] = new SVO(2);
	root->children[7]->children[0] = new SVO(1);
	root->children[7]->children[7] = new SVO(1);
	return root;
}

SVO* SVO::terrain(int size) {
	Noise noise;
	SVO* root = new SVO(size);
	for (int x = 0; x != size; ++x) {
		for (int z = 0; z != size; ++z) {
			const auto height = std::min(noise(x, z), size);
			for (int y = 0; y < height; ++y) {
				root->set(x, y, z, {});
			}
		}
	}
	return root;
}

SVO* SVO::stair(int size) {
	SVO* root = new SVO(size);
	for (int x = 0; x != size; ++x) {
		for (int z = 0; z != size; ++z) {
			for (int y = 0; y < std::min(x + z, size); ++y) {
				root->set(x, y, z, {});
			}
		}
	}
	return root;
}

void SVO::set(size_t x, size_t y, size_t z, glm::uvec3 rgb) {
	assert(x < size && y < size && z < size);
	if (size == 1) {
		assert(x == 0 && y == 0 && z == 0);
		//children[index]->color = rgb;
		return;
	}

	size_t index = int(x / float(size) * 2) * 4 + int(y / float(size) * 2) * 2 + int(z / float(size) * 2);
	assert(index >= 0 && index <= 7);
	if (!children[index]) children[index] = new SVO(size / 2);
	children[index]->set(x % (size / 2), y % (size / 2), z % (size / 2), rgb);
}

void SVO::toSVDAG(std::vector<int32_t>& result) {
	std::unordered_map<size_t, size_t> hashToIndex;
	toSVDAGImpl(result, hashToIndex);
}

void SVO::toSVDAGImpl(std::vector<int32_t>& result,	std::unordered_map<size_t, size_t>& hashToIndex) {
	int bitmask = 0;
	int bitmaskIndex = result.size();
	result.push_back(-1); // placeholder for bitmask
	for (int i = 0; i < 8; i++) {
		if (children[i] == nullptr) continue;
		result.push_back(-1); // placeholder for children index
		bitmask |= (1 << i);
	}
	result[bitmaskIndex] = bitmask; // fill bitmask

	int cnt = 0;
	for (int i = 0; i < 8; i++) {
		if (children[i] == nullptr) continue;

		int childrenPos;
		size_t hash = children[i]->hash();
		// DAG optimization
		if (hashToIndex.find(hash) != hashToIndex.end()) {
			childrenPos = hashToIndex[hash];
		}
		else {
			childrenPos = result.size();
			hashToIndex[hash] = childrenPos;
			children[i]->toSVDAGImpl(result, hashToIndex);
		}
		result[bitmaskIndex + 1 + cnt++] = childrenPos; // fill children index
	}
}


size_t SVO::hash() {
	static std::hash<long> hasher;
	if(hashValue) return hashValue;
	
	long result = 0;
	
	for (int i = 0; i < 8; i++) {
		if (children[i] != nullptr) {
			result += children[i]->hash();
		}
		else {
			result += 1 << i;
		}
	}

	return hashValue = hasher(result);
}
