#include "pch.h"
#include "BaseScene.h"
#include "ISceneFactory.h"
#include "SceneManager.h"
#include <EngineContext.h>

namespace GameEngine {
bool SceneManager::ChangeScene(const std::string& name) {
   if (!factory_ || name.empty() || isChangingScene_) {
	  return false;
   }

   auto newScene = factory_->CreateScene(name);
   if (!newScene) {
	  return false;
   }

   newScene->SetEditorSceneName(name);
   if (!ChangeScene(std::move(newScene))) {
	  return false;
   }

   currentSceneName_ = name;
   return true;
}

bool SceneManager::ChangeScene(std::unique_ptr<BaseScene> newScene) {
   if (!newScene || isChangingScene_) {
	  return false;
   }

   isChangingScene_ = true;

   if (currentScene_) {
	  currentScene_->Finalize();
	  currentScene_.reset();
   }

   currentScene_ = std::move(newScene);
   currentScene_->Initialize();
#ifdef USE_IMGUI
   currentScene_->LoadEditorSceneIfNeeded();
#endif

   isChangingScene_ = false;
   return true;
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
   if (!currentScene_ || isChangingScene_) return;

   std::string nextSceneName = currentScene_->GetNextSceneName();
   if (nextSceneName.empty()) {
	  return;
   }

   if (nextSceneName == currentSceneName_) {
	  BaseScene::SetNextSceneName("");
	  return;
   }

   if (!ChangeScene(nextSceneName)) {
	  BaseScene::SetNextSceneName("");
   }
}
}
