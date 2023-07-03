#include "SVO.h"
#include <functional>

SVO* SVO::sample() {
	SVO* root = new SVO();
	root->children[1] = new SVO();
	//root->children[7] = new SVO();
	//root->children[7]->children[0] = new SVO();
	//root->children[7]->children[7] = new SVO();
	return root;
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
