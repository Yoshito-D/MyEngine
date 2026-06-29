#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace GameEngine {

class SceneManager;

enum class PlayMode {
   Edit,
   Playing,
   Paused,
};

const char* ToString(PlayMode mode);

class PlayModeController {
public:
   void RequestPlay();
   void RequestStop();
   void RequestPause();
   void RequestResume();
   void RequestStep();

   void ProcessRequests(SceneManager& sceneManager);

   PlayMode GetMode() const { return mode_; }
   bool IsPlaying() const { return mode_ == PlayMode::Playing; }
   bool IsPaused() const { return mode_ == PlayMode::Paused; }
   bool IsInPlayMode() const { return mode_ != PlayMode::Edit; }
   bool ShouldRunRuntimeUpdate() const { return shouldRunRuntimeUpdate_; }
   float GetGameDeltaTime() const { return gameDeltaTime_; }
   float GetTimeScale() const { return timeScale_; }
   void SetTimeScale(float timeScale);

private:
   void StartPlaying(SceneManager& sceneManager);
   void StopPlaying(SceneManager& sceneManager);
   void ClearTransitionRequests();

   PlayMode mode_ = PlayMode::Edit;
   bool playRequested_ = false;
   bool stopRequested_ = false;
   bool pauseRequested_ = false;
   bool resumeRequested_ = false;
   bool stepRequested_ = false;
   bool shouldRunRuntimeUpdate_ = false;
   float gameDeltaTime_ = 0.0f;
   float timeScale_ = 1.0f;
   float stepDeltaTime_ = 1.0f / 60.0f;

   std::string playStartSceneName_;
   nlohmann::json editorSceneSnapshot_;
   bool hasEditorSceneSnapshot_ = false;
   bool playStartSceneWasDirty_ = false;
};

} // namespace GameEngine
