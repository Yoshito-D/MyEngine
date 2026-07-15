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
   // @brief PlayModeController のコンストラクタ
   void RequestPlay();

   // @brief PlayModeController の停止要求
   void RequestStop();

   // @brief PlayModeController の一時停止要求
   void RequestPause();

   // @brief PlayModeController の再開要求
   void RequestResume();

   // @brief PlayModeController のステップ要求
   void RequestStep();

   // @brief PlayModeController の更新処理
   // @param sceneManager シーンマネージャー
   void ProcessRequests(SceneManager& sceneManager);

   // @brief プレイモードの状態を取得
   PlayMode GetMode() const { return mode_; }

   // @brief プレイモードが再生中かどうかを取得
   bool IsPlaying() const { return mode_ == PlayMode::Playing; }

   // @brief プレイモードが一時停止中かどうかを取得
   bool IsPaused() const { return mode_ == PlayMode::Paused; }

   // @brief プレイモードが編集モードかどうかを取得
   bool IsInPlayMode() const { return mode_ != PlayMode::Edit; }

   // @brief ランタイム更新を実行すべきかどうかを取得
   bool ShouldRunRuntimeUpdate() const { return shouldRunRuntimeUpdate_; }

   // @brief ゲーム用デルタタイムを取得
   float GetGameDeltaTime() const { return gameDeltaTime_; }

   // @brief タイムスケールを取得
   float GetTimeScale() const { return timeScale_; }

   // @brief タイムスケールを設定
   void SetTimeScale(float timeScale);

   /// @brief 新しいシーンを初期化する前にプレイモードを停止する
   /// @details 通常の停止と異なり、再生開始時のシーンは復元しない
   void StopForSceneInitialization();

private:
   // @brief プレイモードの開始処理
   // @param sceneManager シーンマネージャー
   void StartPlaying(SceneManager& sceneManager);

   // @brief プレイモードの停止処理
   // @param sceneManager シーンマネージャー
   void StopPlaying(SceneManager& sceneManager);

   // @brief プレイモードの一時停止処理
   void ClearTransitionRequests();

   // @brief 再生開始時に保存したシーン情報を破棄する
   void ClearPlaySessionState();

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
