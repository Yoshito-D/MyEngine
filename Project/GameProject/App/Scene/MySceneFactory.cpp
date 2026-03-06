#include "MySceneFactory.h"
#include "ClearScene.h"
#include "GameOverScene.h"
#include "TestScene.h"

using namespace GameEngine;
std::unique_ptr<BaseScene> MySceneFactory::CreateScene(const std::string& name) {
	if (name == "Title") {
		auto scene = std::make_unique<TitleScene>();
		return scene;
	}
	
	if (name == "Game") {
		auto scene = std::make_unique<GameScene>();
		return scene;
	}

	if (name == "Clear") {
		auto scene = std::make_unique<ClearScene>();
		return scene;
	}

	if (name == "GameOver") {
		auto scene = std::make_unique<GameOverScene>();
		return scene;
	}

	if (name == "Test") {
		auto scene = std::make_unique<TestScene>();
		return scene;
	}

	return nullptr;
}