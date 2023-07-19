#pragma once
#include "SVO.h"
#include <string>

class Scene {
public:
	virtual ~Scene() {}
	virtual SVO* load(int param) = 0;
	virtual const char* getDisplayName() = 0;
	virtual bool hasParam() { return false; }
	virtual const char* getParamName() { return nullptr; }
};

class TestScene: public Scene {
public:
	TestScene() = default;
	~TestScene() { delete scene; }
	
	SVO* load(int param) override {
		return scene ? scene : (scene = SVO::sample());
	}

	const char* getDisplayName() override {
		return "Test";
	}

private:
	SVO* scene = nullptr;
};

class TerrainScene : public Scene {
public:
	TerrainScene() = default;
	~TerrainScene() { delete scene; }
	SVO* load(int param) override {
		if (scene) delete scene;
		return scene = SVO::terrain(param);
	}
	const char* getDisplayName() override {
		return "Terrain";
	}
	bool hasParam() override { return true; }
	const char* getParamName() override { return "Terrain Size"; }
private:
	SVO* scene = nullptr;
};

class VoxModelScene : public Scene {
public:
	VoxModelScene(std::string path) : path(std::move(path)) {}
	~VoxModelScene() { delete scene; }
	SVO* load(int param) override {
		return scene ? scene: (scene = SVO::fromVox(path.c_str()));
	}
	const char* getDisplayName() override {
		return path.c_str();
	}
private:
	SVO* scene = nullptr;
	std::string path;
};
