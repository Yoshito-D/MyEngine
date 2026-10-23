#pragma once

#include "GameEngine/Object/Component/Base/IObjectComponent.h"
#include <cstddef>
#include <string>
#include <vector>

namespace App {

class CameraModeSwitcher;
class GravityBody;
class VehicleController;
class VehicleSpeedPostEffectController;

/// @brief タイムアタックの状態、経過時間、チェックポイント進行を管理する
class RaceManagerComponent final : public GameEngine::IObjectComponent {
public:
   /// @brief レース進行状態
   enum class State {
      Waiting,
      Countdown,
      Running,
      Finished,
   };

   /// @brief レース開始方式
   enum class StartMode {
      Immediate,
      Countdown,
      Gate,
   };

   static constexpr const char* kTypeName = "RaceManagerComponent"; ///< コンポーネント型名。
   /// @brief エディターに表示する日本語名と英語名。
   static constexpr GameEngine::ComponentDisplayName kDisplayName{"レース管理", "Race Manager"};

   /// @brief 保存・表示するベストタイムの最大件数
   static constexpr size_t kBestTimeCount = 3;

   /// @brief コンポーネント型名を取得する
   /// @return RaceManagerComponent
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief レース状態と計測時間を更新する
   /// @param deltaTime ゲーム用デルタタイム（秒）
   void Update(float deltaTime) override;

   /// @brief シーン内のプレイヤー参照と記録を復元し、開始状態へリセットする
   /// @param sceneWorld 所属するシーンワールド
   void OnSceneLoaded(GameEngine::SceneWorld& sceneWorld) override;

   /// @copydoc GameEngine::IObjectComponent::OnReferencesChanged
   void OnReferencesChanged(GameEngine::SceneWorld& sceneWorld) override;

   /// @brief スタートゲート通過を通知する
   void NotifyStart();

   /// @brief 指定番号のチェックポイント通過を通知する
   /// @param checkpointIndex 0始まりのチェックポイント番号
   void NotifyCheckpoint(size_t checkpointIndex);

   /// @brief ゴールゲート通過を通知する
   void NotifyFinish();

   /// @brief スタート兼ゴールゲート通過を通知する
   void NotifyStartFinish();

   /// @brief スタート兼ゴールゲートから退出したことを通知する
   void NotifyStartGateExit();

   /// @brief レースを初期状態へ戻す
   void Restart();

   /// @brief 現在のレース状態を取得する
   /// @return レース進行状態
   State GetState() const { return state_; }

   /// @brief 現在の計測時間を取得する
   /// @return 秒単位の経過時間
   float GetElapsedTime() const { return elapsedTime_; }

   /// @brief 現在のベストタイムを取得する
   /// @return 記録がない場合は0、記録がある場合は秒単位のタイム
   float GetBestTime() const { return bestTimes_.empty() ? 0.0f : bestTimes_.front(); }

   /// @brief 保存済みベストタイムを速い順で取得する
   /// @return 最大3件の秒単位タイム
   const std::vector<float>& GetBestTimes() const { return bestTimes_; }

   /// @brief カウントダウン残り時間を取得する
   /// @return 秒単位の残り時間
   float GetCountdownRemaining() const { return countdownRemaining_; }

   /// @brief START表示期間中か調べる
   /// @return STARTを表示する場合はtrue
   bool IsStartBannerVisible() const { return state_ == State::Running && startBannerRemaining_ > 0.0f; }

   /// @brief 設定されたレースシーンの完全な再読み込みを要求する
   /// @return シーン遷移要求を受け付けた場合はtrue
   bool RequestRestart();

   /// @brief レース設定をJSONへ保存する
   /// @return 保存用JSON
   nlohmann::json Serialize() const override;

   /// @brief JSONからレース設定を読み込む
   /// @param data レース設定JSON
   void Deserialize(const nlohmann::json& data) override;

#ifdef USE_IMGUI

   /// @brief レース状態と設定をインスペクターへ表示する
   void DrawInspector() override;
#endif

private:
   void BeginRace();
   bool CanFinish() const;
   void SetPlayerLocked(bool locked);
   void SetCameraMotionPaused(bool paused);
   void AddBestTime(float timeSeconds);
   bool LoadBestTimes();
   bool SaveBestTimes() const;
   void LogRecordWarning(const std::string& message) const;

   State state_ = State::Waiting;                       ///< 現在のレース状態。
   StartMode startMode_ = StartMode::Countdown;         ///< 即時・カウントダウン・ゲート通過の開始方式。
   float elapsedTime_ = 0.0f;                           ///< 現在の計測時間（秒）。
   std::vector<float> bestTimes_;                       ///< 保存済みベストタイム（秒）を速い順で保持する一覧。
   float countdownSeconds_ = 3.0f;                      ///< 開始前カウントダウンの時間（秒）。
   float countdownRemaining_ = 3.0f;                    ///< カウントダウン残り時間（秒）。
   float startTextDuration_ = 0.8f;                     ///< スタート表示を維持する時間（秒）。
   float startBannerRemaining_ = 0.0f;                  ///< スタート表示の残り時間（秒）。
   size_t checkpointCount_ = 0;                         ///< ゴール成立に必要なチェックポイントの総数。
   size_t nextCheckpointIndex_ = 0;                     ///< 次に通過する必要があるチェックポイント番号。
   bool startGateExited_ = false;                       ///< スタート後に共用ゲートの外へ一度出たかどうか。
   std::string playerObjectId_;                         ///< 操作対象プレイヤーの安定Entity ID。
   std::string finishCameraId_;                         ///< ゴール後の表示に使う仮想カメラの安定ID。
   std::string restartScene_;                           ///< リトライ時に読み込む登録シーン名。
   std::string recordKey_;                              ///< 保存記録をコースやルールごとに区別するキー。
   std::string recordFile_ = "Saved/race_records.json"; ///< ベストタイム一覧を保存するJSONファイル。
   bool lockPlayerDuringCountdown_ = true;              ///< カウントダウン中にプレイヤー操作を停止するかどうか。
   bool lockPlayerOnFinish_ = true;                     ///< ゴール後にプレイヤー操作を停止するかどうか。
   std::string nextScene_;                              ///< 進行完了後に読み込む登録シーン名。
   float finishDelay_ = 0.0f;                           ///< ゴールから次シーンへ遷移するまでの待機時間（秒）。
   float finishElapsed_ = 0.0f;                         ///< ゴール後の遷移待機で経過した時間（秒）。
   bool sceneChangeRequested_ = false;                  ///< この進行状態からシーン切り替えを要求済みかどうか。
   bool restartRequested_ = false;                      ///< リトライのシーン切り替えを要求済みかどうか。
   bool runtimeInitialized_ = false;                    ///< 最初の実行時更新に必要な初期化が完了したかどうか。
   bool playerLocked_ = false;                          ///< レース進行によるプレイヤーの一時停止が適用中かどうか。
   bool vehicleControllerEnabledWhenUnlocked_ = false;  ///< 操作ロック解除時に戻す車両制御の元の有効状態。
   bool gravityBodyEnabledWhenUnlocked_ = false;        ///< 操作ロック解除時に戻す重力処理の元の有効状態。
   bool cameraSwitcherEnabledWhenUnlocked_ = false;     ///< 操作ロック解除時に戻すカメラ切り替えの元の有効状態。
   bool speedPostEffectEnabledWhenRacing_ = false;      ///< レース再開時に戻す速度演出制御の元の有効状態。
   /// @brief プレイヤーの車両操作を制御するコンポーネントへの非所有参照。
   VehicleController* vehicleController_ = nullptr;
   GravityBody* gravityBody_ = nullptr;           ///< プレイヤーの速度・重力・姿勢を参照するコンポーネント。
   CameraModeSwitcher* cameraSwitcher_ = nullptr; ///< プレイヤーの仮想カメラ切り替えを行うコンポーネントへの参照。
   /// @brief プレイヤー速度に応じた画面演出を制御するコンポーネントへの参照。
   VehicleSpeedPostEffectController* speedPostEffectController_ = nullptr;
};

} // namespace App
