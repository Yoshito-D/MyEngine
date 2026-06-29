#include "pch.h"
#include "PlayModeController.h"

#include "EngineContext.h"
#include "SceneManager.h"
#include "BaseScene.h"

#ifdef USE_IMGUI
#include "Editor/EditorSceneContext.h"
#endif

#include <algorithm>

namespace GameEngine {

const char* ToString(PlayMode mode) {
   switch (mode) {
      case PlayMode::Playing:
         return "Playing";
      case PlayMode::Paused:
         return "Paused";
      case PlayMode::Edit:
      default:
         return "Edit";
   }
}

void PlayModeController::RequestPlay() {
   if (mode_ == PlayMode::Paused) {
      resumeRequested_ = true;
      return;
   }
   playRequested_ = true;
}

void PlayModeController::RequestStop() {
   stopRequested_ = true;
}

void PlayModeController::RequestPause() {
   pauseRequested_ = true;
}

void PlayModeController::RequestResume() {
   resumeRequested_ = true;
}

void PlayModeController::RequestStep() {
   stepRequested_ = true;
}

void PlayModeController::ProcessRequests(SceneManager& sceneManager) {
   shouldRunRuntimeUpdate_ = false;
   gameDeltaTime_ = 0.0f;

   if (stopRequested_) {
      StopPlaying(sceneManager);
      ClearTransitionRequests();
   } else {
      if (playRequested_) {
         if (mode_ == PlayMode::Edit) {
            StartPlaying(sceneManager);
         } else if (mode_ == PlayMode::Paused) {
            mode_ = PlayMode::Playing;
         }
      }

      if (pauseRequested_ && mode_ == PlayMode::Playing) {
         mode_ = PlayMode::Paused;
      }

      if (resumeRequested_ && mode_ == PlayMode::Paused) {
         mode_ = PlayMode::Playing;
      }
   }

   const bool consumeStep = stepRequested_ && mode_ == PlayMode::Paused;
   ClearTransitionRequests();

   if (mode_ == PlayMode::Playing) {
      gameDeltaTime_ = EngineContext::GetUnscaledDeltaTime() * timeScale_;
      shouldRunRuntimeUpdate_ = true;
   } else if (consumeStep) {
      gameDeltaTime_ = stepDeltaTime_ * timeScale_;
      shouldRunRuntimeUpdate_ = true;
   }

   EngineContext::SetGameDeltaTime(gameDeltaTime_);
}

void PlayModeController::SetTimeScale(float timeScale) {
   timeScale_ = std::max(0.0f, timeScale);
}

void PlayModeController::StartPlaying(SceneManager& sceneManager) {
   if (mode_ != PlayMode::Edit) {
      return;
   }

   playStartSceneName_ = sceneManager.GetCurrentSceneName();
   editorSceneSnapshot_ = nlohmann::json();
   hasEditorSceneSnapshot_ = false;
   playStartSceneWasDirty_ = false;

#ifdef USE_IMGUI
   if (BaseScene* scene = sceneManager.GetCurrentScene()) {
      if (EditorSceneContext* editorContext = scene->GetEditorSceneContext()) {
         editorSceneSnapshot_ = editorContext->SerializeToJson();
         hasEditorSceneSnapshot_ = editorSceneSnapshot_.is_object();
         playStartSceneWasDirty_ = editorContext->IsDirty();
      }
   }
#endif

   mode_ = PlayMode::Playing;
}

void PlayModeController::StopPlaying(SceneManager& sceneManager) {
   if (mode_ == PlayMode::Edit) {
      return;
   }

   mode_ = PlayMode::Edit;

#ifdef USE_IMGUI
   if (!playStartSceneName_.empty() && hasEditorSceneSnapshot_) {
      if (sceneManager.ChangeScene(playStartSceneName_)) {
         if (BaseScene* scene = sceneManager.GetCurrentScene()) {
            if (EditorSceneContext* editorContext = scene->GetEditorSceneContext()) {
               if (editorContext->LoadFromJson(editorSceneSnapshot_)) {
                  if (playStartSceneWasDirty_) {
                     editorContext->MarkDirty();
                  } else {
                     editorContext->ClearDirty();
                  }
               }
            }
         }
      }
   }
#endif

   playStartSceneName_.clear();
   editorSceneSnapshot_ = nlohmann::json();
   hasEditorSceneSnapshot_ = false;
   playStartSceneWasDirty_ = false;
}

void PlayModeController::ClearTransitionRequests() {
   playRequested_ = false;
   stopRequested_ = false;
   pauseRequested_ = false;
   resumeRequested_ = false;
   stepRequested_ = false;
}

} // namespace GameEngine
