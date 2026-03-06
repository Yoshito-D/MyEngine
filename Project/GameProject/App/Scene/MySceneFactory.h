#pragma once
#include "Scene/ISceneFactory.h"
#include "GameScene.h"
#include "TitleScene.h"
#include "ClearScene.h"
#include "GameOverScene.h"
#include <memory>

class MySceneFactory : public GameEngine::ISceneFactory {
public:
	std::unique_ptr<GameEngine::BaseScene> CreateScene(const std::string& name) override;
};
