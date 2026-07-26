#pragma once

#include <nlohmann/json.hpp>
#include <string>

namespace GameEngine {

class SceneManager;

/// @brief エディターの実行状態
enum class PlayMode {
   Edit,
   Playing,
   Paused,
};

/// @brief プレイモードを表示用文字列へ変換する
const char* ToString(PlayMode mode);

/// @brief 編集・再生・一時停止の遷移とゲーム時間を管理する
class PlayModeController {
public:
   /// @brief 再生開始を次回更新へ予約する
   void RequestPlay();

   /// @brief 再生停止を次回更新へ予約する
   void RequestStop();

   /// @brief 一時停止を次回更新へ予約する
   void RequestPause();

   /// @brief 再開を次回更新へ予約する
   void RequestResume();

   /// @brief 一時停止中の1フレーム実行を予約する
   void RequestStep();

   /// @brief 予約された状態遷移を処理してゲーム時間を更新する
   /// @param sceneManager シーンの保存と復元に使用するマネージャー
   void ProcessRequests(SceneManager& sceneManager);

   /// @brief 現在のプレイモードを取得する
   PlayMode GetMode() const { return mode_; }

   /// @brief プレイモードが再生中かを取得する
   bool IsPlaying() const { return mode_ == PlayMode::Playing; }

   /// @brief プレイモードが一時停止中かを取得する
   bool IsPaused() const { return mode_ == PlayMode::Paused; }

   /// @brief 編集モード以外かを取得する
   bool IsInPlayMode() const { return mode_ != PlayMode::Edit; }

   /// @brief 現在のフレームでランタイム更新を実行すべきか取得する
   bool ShouldRunRuntimeUpdate() const { return shouldRunRuntimeUpdate_; }

   /// @brief タイムスケールとプレイ状態を反映したデルタタイムを取得する
   float GetGameDeltaTime() const { return gameDeltaTime_; }

   /// @brief ゲーム時間へ適用する倍率を取得する
   float GetTimeScale() const { return timeScale_; }

   /// @brief ゲーム時間へ適用する倍率を設定する
   void SetTimeScale(float timeScale);

   /// @brief 新しいシーンを初期化する前にプレイモードを停止する
   /// @details 通常の停止と異なり、再生開始時のシーンは復元しない
   void StopForSceneInitialization();

private:
   /// @brief プレイモードの開始処理
   /// @param sceneManager シーンマネージャー
   void StartPlaying(SceneManager& sceneManager);

   /// @brief プレイモードの停止処理
   /// @param sceneManager シーンマネージャー
   void StopPlaying(SceneManager& sceneManager);

   /// @brief 未処理の状態遷移要求を破棄する
   void ClearTransitionRequests();

   /// @brief 再生開始時に保存したシーン情報を破棄する
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
