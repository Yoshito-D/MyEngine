#include "MySceneFactory.h"
#include "GameTestScene.h"
#include "EngineTestScene.h"

using namespace GameEngine;
std::unique_ptr<BaseScene> MySceneFactory::CreateScene(const std::string& name) {

	if (name == "Test") {
		auto scene = std::make_unique<GameTestScene>();
		return scene;
	}

	if (name == "EngineTest") {
		auto scene = std::make_unique<EngineTestScene>();
		return scene;
	}

	return nullptr;
}