#include "pch.h"
#include "BaseScene.h"
#include "ISceneFactory.h"
#include "SceneManager.h"
#include <EngineContext.h>
#include <algorithm>

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

#ifdef USE_IMGUI
   EngineContext::StopPlayModeForSceneInitialization();
#endif

   currentScene_ = std::move(newScene);
   currentScene_->Initialize();
   currentScene_->LoadSceneDataIfNeeded();

   isChangingScene_ = false;
   BeginFadeIn();
   return true;
}

void SceneManager::Update() {
   UpdateTransition(EngineContext::GetUnscaledDeltaTime());
   if (currentScene_) { currentScene_->Update(); }
}

void SceneManager::EditorUpdate() {
   UpdateTransition(EngineContext::GetUnscaledDeltaTime());
   if (currentScene_) { currentScene_->EditorUpdate(); }
}

void SceneManager::RuntimeUpdate() {
   if (currentScene_) { currentScene_->RuntimeUpdate(); }
}

void SceneManager::Draw() {
   if (currentScene_) currentScene_->Draw();
}

void SceneManager::Finalize() {
   if (currentScene_) {
	  currentScene_->Finalize();
	  currentScene_.reset();
   }
   pendingSceneName_.clear();
   transitionState_ = TransitionState::Idle;
   transitionElapsed_ = 0.0f;
   transitionOpacity_ = 0.0f;
   EngineContext::SetSceneTransitionOpacity(0.0f);
}

void SceneManager::CheckSceneChange() {
   if (!currentScene_ || isChangingScene_) return;

   std::string nextSceneName = currentScene_->GetNextSceneName();
   if (nextSceneName.empty()) {
	  return;
   }

   if (pendingSceneName_.empty()) {
      StartFadeOut(nextSceneName);
   }
}

void SceneManager::UpdateTransition(float deltaTime) {
   if (transitionState_ == TransitionState::Idle) {
      return;
   }

   transitionElapsed_ += std::max(deltaTime, 0.0f);
   const float progress = std::clamp(transitionElapsed_ / kTransitionDuration, 0.0f, 1.0f);
   if (transitionState_ == TransitionState::FadingIn) {
      transitionOpacity_ = 1.0f - progress;
      EngineContext::SetSceneTransitionOpacity(transitionOpacity_);
      if (progress >= 1.0f) {
         transitionState_ = TransitionState::Idle;
      }
      return;
   }

   transitionOpacity_ = progress;
   EngineContext::SetSceneTransitionOpacity(transitionOpacity_);
   if (progress < 1.0f || pendingSceneName_.empty()) {
      return;
   }

   const std::string nextSceneName = std::move(pendingSceneName_);
   pendingSceneName_.clear();

#ifdef USE_IMGUI
   const bool resumePlayMode = EngineContext::IsPlaying();
#endif

   // 完全に暗転してから同名シーンも作り直し、切り替え後は黒からフェードインする。
   if (!ChangeScene(nextSceneName)) {
      BeginFadeIn();
      BaseScene::SetNextSceneName("");
      return;
   }

#ifdef USE_IMGUI
   if (resumePlayMode) {
      // シーン初期化で一時停止したプレイモードを次フレーム開始時に復帰させる。
      EngineContext::RequestPlayModeStart();
   }
#endif
}

void SceneManager::StartFadeOut(const std::string& nextSceneName) {
   if (nextSceneName.empty()) {
      return;
   }
   pendingSceneName_ = nextSceneName;
   BaseScene::SetNextSceneName("");
   transitionState_ = TransitionState::FadingOut;
   // フェードイン中の再要求でも現在の明るさから連続して暗転させる。
   transitionElapsed_ = transitionOpacity_ * kTransitionDuration;
   EngineContext::SetSceneTransitionOpacity(transitionOpacity_);
}

void SceneManager::BeginFadeIn() {
   transitionState_ = TransitionState::FadingIn;
   transitionElapsed_ = 0.0f;
   transitionOpacity_ = 1.0f;
   EngineContext::SetSceneTransitionOpacity(transitionOpacity_);
}
} // namespace GameEngine
