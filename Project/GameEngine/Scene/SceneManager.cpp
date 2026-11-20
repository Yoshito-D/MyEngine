#include "GameEngine/pch.h"
#include "GameEngine/Scene/SceneManager.h"
#include "GameEngine/Audio/Audio.h"
#include "GameEngine/Graphics/Renderer/Renderer.h"
#include "GameEngine/Object/Component/Audio/AudioSceneLifecycle.h"
#include <fstream>
#include <utility>

namespace GameEngine {
SceneManager::SceneManager(
   SceneCatalog& catalog, AssetManager& assets, Audio& audio, CameraManager& cameras, Renderer& renderer)
   : assets_(assets), audio_(audio), cameras_(cameras), renderer_(renderer), catalog_(catalog) {
   ConfigureTransitionStates();
   transitionMachine_.Start(TransitionState::Idle);
}

void SceneManager::ConfigureTransitionStates() {
   transitionMachine_.AddState(TransitionState::Idle, {.onEnter = [this]() {
      elapsed_ = opacity_ = 0.0f;
      renderer_.SetSceneTransitionOpacity(opacity_);
   }});

   StateMachine<TransitionState>::State fadingOut = {
      .onEnter =
         [this]() {
            elapsed_ = opacity_ * kTransitionDuration;
            transitionUpdateTime_ = std::chrono::steady_clock::now();
         },
      .onUpdate = [this](float seconds) { UpdateFade(seconds, true); },
   };
   transitionMachine_.AddState(TransitionState::FadingOut, std::move(fadingOut));
   transitionMachine_.AddState(TransitionState::WaitingForCommit, {});
   transitionMachine_.AddState(TransitionState::PreparingFadeIn, {.onEnter = [this]() {
      elapsed_ = 0.0f;
      opacity_ = 1.0f;
      renderer_.SetSceneTransitionOpacity(opacity_);
   }});

   StateMachine<TransitionState>::State fadingIn = {
      .onEnter =
         [this]() {
            elapsed_ = 0.0f;
            transitionUpdateTime_ = std::chrono::steady_clock::now();
         },
      .onUpdate = [this](float seconds) { UpdateFade(seconds, false); },
   };
   transitionMachine_.AddState(TransitionState::FadingIn, std::move(fadingIn));
   transitionMachine_.SetTransitionRule(
      TransitionState::Idle, {TransitionState::FadingOut, TransitionState::PreparingFadeIn});
   transitionMachine_.SetTransitionRule(TransitionState::FadingOut, {TransitionState::WaitingForCommit});
   transitionMachine_.SetTransitionRule(TransitionState::WaitingForCommit, {});
   transitionMachine_.SetTransitionRule(TransitionState::PreparingFadeIn, {TransitionState::FadingIn});
   transitionMachine_.SetTransitionRule(TransitionState::FadingIn, {TransitionState::Idle});
}

void SceneManager::UpdateFade(float deltaTime, bool fadingOut) {
   elapsed_ += deltaTime;
   const float progress = std::clamp(elapsed_ / kTransitionDuration, 0.0f, 1.0f);
   opacity_ = fadingOut ? progress : 1.0f - progress;
   renderer_.SetSceneTransitionOpacity(opacity_);

   if (progress >= 1.0f) {
      transitionMachine_.RequestState(fadingOut ? TransitionState::WaitingForCommit : TransitionState::Idle);
   }
}

SceneManager::~SceneManager() {
   // 例外による終了時は、アプリケーション所有の通知先が先に破棄されている可能性がある。
   beforeReplace_ = {};
   Finalize();
}

void SceneManager::SetLifecycleCallbacks(std::function<void()> before,
   std::function<void(Scene&, const SceneSnapshot&)> after, std::function<SceneRecovery(const Scene&)> recovery) {
   beforeReplace_ = std::move(before);
   afterReplace_ = std::move(after);
   captureRecovery_ = std::move(recovery);
}

std::string SceneManager::GetCurrentSceneName() const {
   return currentScene_ ? currentScene_->GetDefinition().name : std::string{};
}

bool SceneManager::IsSceneTransitioning() const {
   return replacing_ || pending_.has_value() || transitionMachine_.GetCurrentState() != TransitionState::Idle;
}

SceneLoadResult SceneManager::Report(SceneLoadStatus status, std::string message) {
   lastRequestError_.clear();
   lastResult_ = {status, std::move(message)};

   if (status != SceneLoadStatus::Succeeded) {
      Logger::Error(lastResult_.message);
   }
   return lastResult_;
}

bool SceneManager::ReadScene(const std::string& name, SceneSnapshot& snapshot, std::string& failure) {
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
      failure = error.what();
      return false;
   }
}

SceneLoadResult SceneManager::LoadInitialScene(const std::string& name) {
   if (currentScene_ || replacing_) {
      return Report(SceneLoadStatus::Rejected, "Startup scene is already loaded");
   }
   SceneSnapshot snapshot;

   std::string failure;

   if (!ReadScene(name, snapshot, failure)) {
      return Report(SceneLoadStatus::Fatal, std::move(failure));
   }
   return Replace(snapshot);
}

SceneRequestStatus SceneManager::RequestSceneChange(const std::string& name) {
   if (!currentScene_ || lastResult_.status == SceneLoadStatus::Fatal) {
      return SceneRequestStatus::Unavailable;
   }

   if (IsSceneTransitioning()) {
      lastRequestError_ = "Scene transition is already pending";
      return SceneRequestStatus::Busy;
   }

   SceneSnapshot snapshot;

   if (!ReadScene(name, snapshot, lastRequestError_)) {
      Logger::Error(lastRequestError_);
      return SceneRequestStatus::InvalidTarget;
   }

   // 予約時に読込と検証を済ませ、暗転後のフレーム境界では確定済みスナップショットを適用する。
   pending_ = std::move(snapshot);
   lastRequestError_.clear();
   return SceneRequestStatus::Accepted;
}

SceneRequestStatus SceneManager::RequestReload() {
   return RequestSceneChange(GetCurrentSceneName());
}

SceneLoadResult SceneManager::RestoreSnapshot(const SceneSnapshot& snapshot) {
   if (replacing_) {
      return Report(SceneLoadStatus::Rejected, "Scene replacement is already running");
   }
   return Replace(snapshot);
}

SceneLoadResult SceneManager::Replace(const SceneSnapshot& snapshot) {
   if (replacing_) {
      return Report(SceneLoadStatus::Rejected, "Reentrant scene replacement");
   }

   replacing_ = true;
   struct ReplacementScope {
      bool& replacing;
      Audio* audio = nullptr;

      void FinishAudio(bool hasBgm) {
         if (auto* pending = std::exchange(audio, nullptr)) {
            pending->EndBgmSceneChange(hasBgm);
         }
      }

      ~ReplacementScope() {
         FinishAudio(false);
         replacing = false;
      }
   } replacement{replacing_};

   // アセット準備に失敗した場合は現在のWorldと実行中の状態を維持する。
   const bool hadScene = currentScene_ != nullptr;

   if (!Scene::PrepareAssets(snapshot.data, assets_)) {
      return Report(hadScene ? SceneLoadStatus::Rejected : SceneLoadStatus::Fatal,
         "Could not prepare scene assets: " + snapshot.definition.name);
   }

   // モードや資源の変更前に、復元用データと編集状態を今回の切替に閉じ込める。
   SceneRecovery recovery;

   try {
      if (hadScene) {
         recovery = captureRecovery_
                       ? captureRecovery_(*currentScene_)
                       : SceneRecovery{{currentScene_->GetDefinition(), currentScene_->GetSceneWorld().SerializeToJson()},
                            {}};
      }
   } catch (const std::exception& error) {
      return Report(SceneLoadStatus::Rejected, error.what());
   }

   CancelTransition();
   audio_.StopPreviews();

   if (audio_.IsAvailable()) {
      // 旧シーンのBGM所有者が解放されても、次のシーンの要求が判明するまで停止判断を保留する。
      audio_.BeginBgmSceneChange();
      replacement.audio = &audio_;
   }

   const auto discardScene = [this]() {
      if (currentScene_ && beforeReplace_) {
         beforeReplace_();
      }

      currentScene_.reset();
   };

   const auto installScene = [this](const SceneSnapshot& source, const auto& attach) {
      currentScene_ = std::make_unique<Scene>(source.definition, cameras_);

      if (!currentScene_->Initialize(source.data)) {
         throw std::runtime_error("Scene construction failed: " + source.definition.name);
      }

      if (attach) {
         attach(*currentScene_, source);
      }
   };

   auto status = SceneLoadStatus::Succeeded;
   std::string description = "Loaded scene: " + snapshot.definition.name;

   try {
      discardScene();
      installScene(snapshot, afterReplace_);
   } catch (const std::exception& error) {
      description = error.what();

      try {
         // 構築途中のシーンと外部接続を外してから、退避した旧シーンを再構築する。
         discardScene();

         if (!hadScene) {
            return Report(SceneLoadStatus::Fatal, std::move(description));
         }

         if (!Scene::PrepareAssets(recovery.snapshot.data, assets_)) {
            throw std::runtime_error("Could not prepare assets for the previous snapshot");
         }

         installScene(recovery.snapshot, recovery.attach);
         status = SceneLoadStatus::Recovered;
         description += "; restored " + recovery.snapshot.definition.name;
      } catch (const std::exception& recoveryError) {
         try {
            discardScene();
         } catch (const std::exception& cleanupError) {
            Logger::Error("Scene detachment failed: " + std::string(cleanupError.what()));
            currentScene_.reset();
         }

         return Report(SceneLoadStatus::Fatal, description + "; recovery failed: " + recoveryError.what());
      }
   }

   // 新規読込か復旧かにかかわらず、実際に有効になったシーンのBGM要求で継続を判断する。
   if (replacement.audio) {
      replacement.FinishAudio(HasSceneBgmRequest(currentScene_->GetSceneWorld()));
   }

#ifndef USE_IMGUI
   // 旧シーンのポーズを引き継ぐと、新しいシーンのBGMや自動再生SEの生成が拒否される。
   audio_.SetGamePaused(false, false);
   BeginSceneAudio(currentScene_->GetSceneWorld());
#endif
   BeginFadeIn();
   return Report(status, std::move(description));
}

void SceneManager::UpdateTransitionFade() {
   const auto state = transitionMachine_.GetCurrentState();

   if (state != TransitionState::FadingOut && state != TransitionState::FadingIn) return;

   // フレームのデルタタイムには開始前のシーン読込も含まれるため、フェード自身の基準時刻から計測する。
   const auto now = std::chrono::steady_clock::now();
   const float deltaTime = std::chrono::duration<float>(now - transitionUpdateTime_).count();
   transitionUpdateTime_ = now;
   transitionMachine_.Update(deltaTime);
   transitionMachine_.ResolveRequests();
}

void SceneManager::CommitPendingSceneChange() {
   if (replacing_) return;

   // 最初の描画で発生する資源準備も暗転中に済ませ、表示できる状態になってから復帰時間を計測する。
   if (transitionMachine_.GetCurrentState() == TransitionState::PreparingFadeIn) {
      transitionMachine_.RequestState(TransitionState::FadingIn);
      transitionMachine_.ResolveRequests();
   }

   // 暗転完了と予約の両方が揃ったフレーム境界でのみ、シーンの実体を入れ替える。
   if (transitionMachine_.GetCurrentState() == TransitionState::WaitingForCommit && pending_) {
      auto snapshot = std::move(*pending_);
      pending_.reset();
      const auto result = Replace(snapshot);

      if (result.status == SceneLoadStatus::Rejected) {
         CancelTransition();
         BeginFadeIn();
      }
   } else if (pending_ && transitionMachine_.GetCurrentState() == TransitionState::Idle) {
      transitionMachine_.RequestState(TransitionState::FadingOut);
      transitionMachine_.ResolveRequests();
   }
}

void SceneManager::CancelTransition() {
   pending_.reset();
   // 取消は通常の遷移規則にかかわらず、予約と表示を初期状態へ戻す。
   transitionMachine_.Stop();
   transitionMachine_.Start(TransitionState::Idle);
}

void SceneManager::BeginFadeIn() {
   transitionMachine_.RequestState(TransitionState::PreparingFadeIn);
   transitionMachine_.ResolveRequests();
}

void SceneManager::Finalize() {
   CancelTransition();

   if (currentScene_ && beforeReplace_) {
      beforeReplace_();
   }
   currentScene_.reset();
}
}
