#pragma once

#include <nlohmann/json.hpp>
#include "GameEngine/Scene/SceneDefinition.h"
#include <string>

namespace GameEngine {

class SceneManager;
class Object;

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

#ifdef USE_IMGUI

   /// @brief プレイ中の指定コンポーネントだけを開始時スナップショットとシーンファイルへ保存する
   /// @param object 保存対象コンポーネントを所有するEntity
   /// @param componentTypeName 保存対象のコンポーネント型名
   /// @return 両方の保存先を更新できた場合はtrue
   bool SaveComponent(Object& object, const std::string& componentTypeName);
#endif

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
   /// @details 開始時のシーン情報を保持し、初期化後の次フレームで元の再生状態へ戻る
   void StopForSceneInitialization();

   /// @brief 復旧に使用できる再生開始時の編集スナップショットがあるか調べる。
   bool HasRecoverySnapshot() const { return hasPlaySession_ && hasEditorSceneSnapshot_; }

   /// @brief カタログやファイルを読み直さず、再生開始時の編集内容を取得する。
   SceneSnapshot GetRecoverySnapshot() const { return {playStartDefinition_, editorSceneSnapshot_}; }

   /// @brief 再生中のシーン切替失敗に備えて、開始時の未保存状態を取得する。
   bool GetPlayStartWasDirty() const { return playStartSceneWasDirty_; }

   /// @brief 読込失敗からの復旧後に再生を停止し、保留中の再開要求を破棄する。
   void StopAfterLoadFailure();

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

   PlayMode mode_ = PlayMode::Edit;      ///< 現在のプレイモード。
   bool playRequested_ = false;          ///< 次のフレーム境界でプレイを開始する要求。
   bool stopRequested_ = false;          ///< 次のフレーム境界でプレイを停止する要求。
   bool pauseRequested_ = false;         ///< 次のフレーム境界で一時停止する要求。
   bool resumeRequested_ = false;        ///< 次のフレーム境界で再生を再開する要求。
   bool stepRequested_ = false;          ///< 次のフレーム境界で1フレームだけ進める要求。
   bool shouldRunRuntimeUpdate_ = false; ///< 現在のフレームでゲーム更新を実行するかどうか。
   float gameDeltaTime_ = 0.0f;          ///< タイムスケールとプレイ状態を反映したデルタタイム（秒）。
   float timeScale_ = 1.0f;              ///< ゲーム時間へ適用する倍率。
   float stepDeltaTime_ = 1.0f / 60.0f;  ///< 1フレーム送りで進める時間（秒）。

   bool hasPlaySession_ = false;         ///< プレイ開始時の復元情報を持つセッションがあるかどうか。
   SceneDefinition playStartDefinition_; ///< プレイ開始時に編集中だったシーンの定義。
   nlohmann::json editorSceneSnapshot_;  ///< プレイ停止時に復元する編集シーンの保存状態。
   bool hasEditorSceneSnapshot_ = false; ///< プレイ開始時の編集シーンを退避済みかどうか。
   bool playStartSceneWasDirty_ = false; ///< 再生中のシーン切替失敗に備えて、開始時の未保存状態。
};

} // namespace GameEngine
