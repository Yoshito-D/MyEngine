#include "pch.h"
#include "BaseScene.h"
#include "ISceneFactory.h"
#include "SceneManager.h"
#include <EngineContext.h>

namespace GameEngine {
void SceneManager::ChangeScene(const std::string& name) {
   auto newScene = factory_->CreateScene(name);
   ChangeScene(std::move(newScene));
   currentSceneName_ = name;
}

void SceneManager::ChangeScene(std::unique_ptr<BaseScene> newScene) {
   if (currentScene_) {
	  currentScene_->Finalize();
   }

   currentScene_ = std::move(newScene);
   if (currentScene_) {
	  currentScene_->Initialize();
   }
}

void SceneManager::Update() {
   if (currentScene_) { currentScene_->Update(); }
}

void SceneManager::Draw() {
   if (currentScene_) currentScene_->Draw();
}

void SceneManager::Finalize() {
   if (currentScene_) {
	  currentScene_->Finalize();
	  currentScene_.reset();
   }
}

void SceneManager::CheckSceneChange() {
   if (!currentScene_) return;

   std::string nextSceneName = currentScene_->GetNextSceneName();
   if (!nextSceneName.empty() && nextSceneName != currentSceneName_) {
	  ChangeScene(nextSceneName);
	  currentSceneName_ = nextSceneName;
   }
}
}