#include "MySceneFactory.h"
#include "TestScene.h"

using namespace GameEngine;
std::unique_ptr<BaseScene> MySceneFactory::CreateScene(const std::string& name) {

	if (name == "Test") {
		auto scene = std::make_unique<TestScene>();
		return scene;
	}

	return nullptr;
}