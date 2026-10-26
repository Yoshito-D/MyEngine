#include "GameEngine/pch.h"
#include "GameEngine/Scene/SceneManager.h"
#include "GameEngine/Framework/EngineContext.h"
#include "GameEngine/Object/Component/Audio/AudioSceneLifecycle.h"
#include <fstream>

namespace GameEngine {
SceneManager::SceneManager(SceneCatalog& catalog) : catalog_(catalog) {
}

SceneManager::~SceneManager() {
   // 例外による終了時は、アプリケーション所有の通知先が先に破棄されている可能性がある。
   beforeReplace_ = {};
   Finalize();
}

void SceneManager::SetLifecycleCallbacks(std::function<void()> before,
   std::function<void(const SceneSnapshot&, bool)> after, std::function<SceneSnapshot()> recovery) {
   beforeReplace_ = std::move(before);
   afterReplace_ = std::move(after);
   captureRecovery_ = std::move(recovery);
}

std::string SceneManager::GetCurrentSceneName() const {
   return currentScene_ ? currentScene_->GetDefinition().name : std::string{};
}

SceneLoadResult SceneManager::Report(SceneLoadStatus status, std::string message) {
   lastResult_ = {status, std::move(message)};

   if (status != SceneLoadStatus::Succeeded) {
      Logger::Error(lastResult_.message);
   }
   return lastResult_;
}

bool SceneManager::ReadScene(const std::string& name, SceneSnapshot& snapshot) {
   try {
      if (!catalog_.Reload()) {
         throw std::runtime_error(catalog_.GetLastError());
      }
      const auto* definition = catalog_.Find(name);

      if (!definition) {
         throw std::runtime_error("Scene is not registered: " + name);
      }
      std::ifstream file(definition->filePath);

      if (!file) {
         throw std::runtime_error("Could not open scene: " + definition->filePath.generic_string());
      }
      snapshot.definition = *definition;
      file >> snapshot.data;

      if (!SceneWorld::IsValidSnapshot(snapshot.data)) {
         throw std::runtime_error("Invalid scene structure or component registration: " + name);
      }
      return true;
   } catch (const std::exception& error) {
      Report(SceneLoadStatus::Rejected, error.what());
      return false;
   }
}

SceneLoadResult SceneManager::LoadInitialScene(const std::string& name) {
   if (currentScene_ || replacing_) {
      return Report(SceneLoadStatus::Rejected, "Startup scene is already loaded");
   }
   SceneSnapshot snapshot;

   if (!ReadScene(name, snapshot)) {
      return Report(SceneLoadStatus::Fatal, lastResult_.message);
   }
   return Replace(snapshot);
}

bool SceneManager::RequestSceneChange(const std::string& name) {
   if (lastResult_.status == SceneLoadStatus::Fatal) {
      return false;
   }

   if (replacing_ || pending_ || transitionState_ != TransitionState::Idle) {
      Report(SceneLoadStatus::Rejected, "Scene transition is already pending");
      return false;
   }
   SceneSnapshot snapshot;

   if (!ReadScene(name, snapshot)) {
      return false;
   }
   pending_ = std::move(snapshot);
   return true;
}

bool SceneManager::RequestReload() {
   return currentScene_ && RequestSceneChange(currentScene_->GetDefinition().name);
}

SceneLoadResult SceneManager::RestoreSnapshot(const SceneSnapshot& snapshot) {
   if (replacing_) {
      return Report(SceneLoadStatus::Rejected, "Scene replacement is already running");
   }
   try {
      if (!SceneWorld::IsValidSnapshot(snapshot.data)) {
         return Report(SceneLoadStatus::Rejected, "Invalid restoration snapshot");
      }
   } catch (const std::exception& error) {
      return Report(SceneLoadStatus::Rejected, error.what());
   }
   return Replace(snapshot);
}

SceneLoadResult SceneManager::Replace(const SceneSnapshot& snapshot) {
   if (replacing_) {
      return Report(SceneLoadStatus::Rejected, "Reentrant scene replacement");
   }
   // モード変更や資源の解放より先に復旧元を確保し、復旧時はシーンファイルを読み直さない。
   SceneSnapshot recovery;
   const bool hadScene = currentScene_ != nullptr;
   try {
      if (hadScene) {
         recovery = captureRecovery_ ? captureRecovery_()
                                     : SceneSnapshot{currentScene_->GetDefinition(),
                                          currentScene_->GetSceneWorld().SerializeToJson()};
      }
   } catch (const std::exception& error) {
      return Report(SceneLoadStatus::Rejected, error.what());
   }
   replacing_ = true;
   CancelTransition();
   auto* audio = EngineContext::GetAudio();

   if (audio) {
      audio->StopPreviews();

      if (audio->IsAvailable()) {
         audio->BeginBgmSceneChange();
      }
   }
   std::string failure;
   try {
      if (beforeReplace_) {
         beforeReplace_();
      }
      currentScene_.reset();
      currentScene_ = std::make_unique<Scene>(snapshot.definition);

      if (!currentScene_->Initialize(snapshot.data)) {
         throw std::runtime_error("Scene construction failed: " + snapshot.definition.name);
      }

      if (afterReplace_) {
         afterReplace_(snapshot, false);
      }
   } catch (const std::exception& error) {
      failure = error.what();
   }

   if (!failure.empty()) {
      if (beforeReplace_) {
         beforeReplace_();
      }
      currentScene_.reset();

      if (!hadScene) {
         if (audio && audio->IsAvailable()) {
            audio->EndBgmSceneChange(false);
         }
         replacing_ = false;
         return Report(SceneLoadStatus::Fatal, failure);
      }
      try {
         currentScene_ = std::make_unique<Scene>(recovery.definition);

         if (!currentScene_->Initialize(recovery.data)) {
            throw std::runtime_error("Could not restore the previous snapshot");
         }

         if (afterReplace_) {
            afterReplace_(recovery, true);
         }
      } catch (const std::exception& error) {
         if (beforeReplace_) {
            beforeReplace_();
         }
         currentScene_.reset();

         if (audio && audio->IsAvailable()) {
            audio->EndBgmSceneChange(false);
         }
         replacing_ = false;
         return Report(SceneLoadStatus::Fatal, failure + "; recovery failed: " + error.what());
      }
   }

   if (audio && audio->IsAvailable()) {
      audio->EndBgmSceneChange(HasSceneBgmRequest(currentScene_->GetSceneWorld()));
   }
#ifndef USE_IMGUI
   BeginSceneAudio(currentScene_->GetSceneWorld());
#endif
   replacing_ = false;
   BeginFadeIn();
   return Report(failure.empty() ? SceneLoadStatus::Succeeded : SceneLoadStatus::Recovered,
      failure.empty() ? "Loaded scene: " + snapshot.definition.name
                      : failure + "; restored " + recovery.definition.name);
}

void SceneManager::EditorUpdate() {
   if (transitionState_ == TransitionState::Idle || transitionState_ == TransitionState::PreparingFadeIn) return;

   // フレームのデルタタイムには開始前のシーン読込も含まれるため、フェード自身の基準時刻から計測する。
   const auto now = std::chrono::steady_clock::now();
   elapsed_ += std::chrono::duration<float>(now - transitionUpdateTime_).count();
   transitionUpdateTime_ = now;
   const float progress = std::clamp(elapsed_ / kTransitionDuration, 0.0f, 1.0f);
   opacity_ = transitionState_ == TransitionState::FadingOut ? progress : 1.0f - progress;
   EngineContext::SetSceneTransitionOpacity(opacity_);

   if (progress >= 1.0f) {
      if (transitionState_ == TransitionState::FadingOut) {
         commitReady_ = true;
      } else {
         transitionState_ = TransitionState::Idle;
      }
   }
}

void SceneManager::RuntimeUpdate() {
   if (currentScene_) {
      currentScene_->RuntimeUpdate(EngineContext::GetDeltaTime());
   }
}

void SceneManager::CheckSceneChange() {
   if (replacing_) return;

   // 最初の描画で発生する資源準備も暗転中に済ませ、表示できる状態になってから復帰時間を計測する。
   if (transitionState_ == TransitionState::PreparingFadeIn) {
      transitionState_ = TransitionState::FadingIn;
      transitionUpdateTime_ = std::chrono::steady_clock::now();
   }

   if (commitReady_ && pending_) {
      auto snapshot = std::move(*pending_);
      pending_.reset();
      const auto result = Replace(snapshot);

      if (result.status == SceneLoadStatus::Rejected) {
         CancelTransition();
         BeginFadeIn();
      }
   } else if (pending_ && transitionState_ != TransitionState::FadingOut) {
      transitionState_ = TransitionState::FadingOut;
      elapsed_ = opacity_ * kTransitionDuration;
      transitionUpdateTime_ = std::chrono::steady_clock::now();
   }
}

void SceneManager::CancelTransition() {
   pending_.reset();
   commitReady_ = false;
   transitionState_ = TransitionState::Idle;
   elapsed_ = opacity_ = 0.0f;
   EngineContext::SetSceneTransitionOpacity(0.0f);
}

void SceneManager::BeginFadeIn() {
   transitionState_ = TransitionState::PreparingFadeIn;
   elapsed_ = 0.0f;
   opacity_ = 1.0f;
   EngineContext::SetSceneTransitionOpacity(opacity_);
}

void SceneManager::Finalize() {
   CancelTransition();

   if (currentScene_ && beforeReplace_) {
      beforeReplace_();
   }
   currentScene_.reset();
}
}
