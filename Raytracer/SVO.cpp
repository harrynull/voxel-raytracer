#include "SVO.h"
#include <functional>
#include "VoxLoader.h"
#include "Terrain.h"

SVO* SVO::sample() {
	SVO* root = new SVO(4);
	root->children[0] = new SVO(2);
	root->children[0]->material.color = { 255, 0, 0 };

	root->children[7] = new SVO(2);
	root->children[7]->children[0] = new SVO(1);
	root->children[7]->children[0]->material.color = { 0, 255, 0 };

	root->children[7]->children[7] = new SVO(1);
	root->children[7]->children[7]->material.color = { 0, 0, 255 };
	root->children[7]->children[7]->material.water = 1;

	root->children[5] = new SVO(2);
	root->children[5]->children[0] = new SVO(1);
	root->children[5]->children[0]->material.color = { 255, 255, 255 };
	root->children[5]->children[0]->material.water = 1;
	return root;
}

SVO* SVO::terrain(int size) {
	constexpr int waterLevel = 32;
	constexpr glm::uvec3 waterColor = { 35, 137, 218 },
		grassColor = { 38, 139, 7 },
		grassColor2 = { 65, 152, 10 },
		dirtColor = { 155, 118, 83 },
		sandColor = { 246,215,176 };

	Noise noise;
	SVO* root = new SVO(size);
	for (int x = 0; x != size; ++x) {
		for (int z = 0; z != size; ++z) {
			const auto height = std::min(noise(x, z), size);
			const bool underWater = height < waterLevel;
			const auto yMax = std::min(size, std::max(waterLevel, height));
			for (int y = 0; y < yMax; ++y) {
				root->set(
					x, y, z,
					//{ x * 256 / size,y * 256 / size,z * 256 / size }
					y >= height ? waterColor :
						(y == height - 1 ?
							(underWater ? sandColor: (rand() > RAND_MAX / 2 ? grassColor : grassColor2)) :
							dirtColor),
					y >= height
				);
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
				root->set(x, y, z, {0,0,255});
			}
		}
	}
	return root;
}

// https://stackoverflow.com/questions/364985/algorithm-for-finding-the-smallest-power-of-two-thats-greater-or-equal-to-a-giv
inline int
pow2roundup(int x) {
	if (x < 0)
		return 0;
	--x;
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> 8;
	x |= x >> 16;
	return x + 1;
}

SVO* SVO::fromVox(const char* filename) {
	int maxX = 0, maxY = 0, maxZ = 0;
	loadVox(filename, [&](int x, int y, int z, glm::uvec3 color) {
		maxX = std::max(maxX, x);
		maxY = std::max(maxY, y);
		maxZ = std::max(maxZ, z);
	});
	int maxOfMax = std::max(std::max(maxX, maxY), maxZ) + 1;

	SVO* root = new SVO(pow2roundup(maxOfMax));
	loadVox(filename, [&](int x, int y, int z, glm::uvec3 color) {
		root->set(x, y, z, color);
		});
	return root;
}

void SVO::set(size_t x, size_t y, size_t z, glm::uvec3 rgb, bool water) {
	assert(x < size && y < size && z < size);
	if (size == 1) {
		assert(x == 0 && y == 0 && z == 0);
		material.color = rgb;
		material.water = water;
		return;
	}

	size_t index = int(x / float(size) * 2) * 4 + int(y / float(size) * 2) * 2 + int(z / float(size) * 2);
	assert(index >= 0 && index <= 7);
	if (!children[index]) children[index] = new SVO(size / 2);
	children[index]->set(x % (size / 2), y % (size / 2), z % (size / 2), rgb, water);
}

void SVO::toSVDAG(std::vector<int32_t>& result, std::vector<Material>& materials) {
	std::unordered_map<size_t, size_t> hashToIndex;
	std::unordered_map<Material, size_t, MaterialHasher> materialToIndex;
	toSVDAGImpl(result, materials, hashToIndex, materialToIndex);
}

void SVO::toSVDAGImpl(
	std::vector<int32_t>& result,
	std::vector<Material>& materials,
	std::unordered_map<size_t, size_t>& hashToIndex,
	std::unordered_map<Material, size_t, MaterialHasher>& materialToIndex
) {
	if (!materialToIndex.contains(material)) {
		materialToIndex[material] = materials.size();
		materials.push_back(material);
	}
	auto matID = materialToIndex[material];
	assert( matID < 1<<24 );

	int bitmask = 0;
	int bitmaskIndex = result.size();
	result.push_back(-1); // placeholder for bitmask
	for (int i = 0; i < 8; i++) {
		if (children[i] == nullptr) continue;
		result.push_back(-1); // placeholder for children index
		bitmask |= (1 << i);
	}
	result[bitmaskIndex] = bitmask | (matID << 8); // fill bitmask

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
			children[i]->toSVDAGImpl(result, materials, hashToIndex, materialToIndex);
		}
		result[bitmaskIndex + 1 + cnt++] = childrenPos; // fill children index
	}
}


size_t SVO::hash() {
	static std::hash<long> hasher;
	if(hashValue) return hashValue;
	
	size_t result = 0;
	
	for (int i = 0; i < 8; i++) {
		if (children[i] != nullptr) {
			result ^= children[i]->hash() << i;
		}
		else {
			result ^= 1 << i;
		}
	}

	return hashValue = (hasher(result) ^ MaterialHasher()(material));
}
