#pragma once

#include "Object/Component/IObjectComponent.h"
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

   static constexpr const char* kTypeName = "RaceManagerComponent";
   static constexpr GameEngine::ComponentDisplayName kDisplayName{ "レース管理", "Race Manager" };

   /// @brief コンポーネント型名を取得する
   /// @return RaceManagerComponent
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief レース状態と計測時間を更新する
   /// @param deltaTime ゲーム用デルタタイム（秒）
   void Update(float deltaTime) override;

   /// @brief シーン内のプレイヤー参照と記録を復元し、開始状態へリセットする
   /// @param sceneWorld 所属するシーンワールド
   void OnSceneLoaded(GameEngine::SceneWorld& sceneWorld) override;

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
   double GetElapsedTime() const { return elapsedTime_; }

   /// @brief 現在のベストタイムを取得する
   /// @return 記録がない場合は0、記録がある場合は秒単位のタイム
   double GetBestTime() const { return bestTimes_.empty() ? 0.0 : bestTimes_.front(); }

   /// @brief 保存済みベストタイムを速い順で取得する
   /// @return 最大3件の秒単位タイム
   const std::vector<double>& GetBestTimes() const { return bestTimes_; }

   /// @brief カウントダウン残り時間を取得する
   /// @return 秒単位の残り時間
   float GetCountdownRemaining() const { return countdownRemaining_; }

   /// @brief START表示期間中か調べる
   /// @return STARTを表示する場合はtrue
   bool IsStartBannerVisible() const { return state_ == State::Running && startBannerRemaining_ > 0.0f; }

   /// @brief 設定されたレースシーンの完全な再読み込みを要求する
   void RequestRestart();

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
   void AddBestTime(double timeSeconds);
   bool LoadBestTimes();
   bool SaveBestTimes() const;
   void LogRecordWarning(const std::string& message) const;

   State state_ = State::Waiting;
   StartMode startMode_ = StartMode::Countdown;
   double elapsedTime_ = 0.0;
   std::vector<double> bestTimes_;
   float countdownSeconds_ = 3.0f;
   float countdownRemaining_ = 3.0f;
   float startTextDuration_ = 0.8f;
   float startBannerRemaining_ = 0.0f;
   size_t checkpointCount_ = 0;
   size_t nextCheckpointIndex_ = 0;
   bool startGateExited_ = false;
   std::string playerObjectId_;
   std::string finishCameraId_;
   std::string restartScene_;
   std::string recordKey_;
   std::string recordFile_ = "Saved/race_records.json";
   bool lockPlayerDuringCountdown_ = true;
   bool lockPlayerOnFinish_ = true;
   std::string nextScene_;
   float finishDelay_ = 0.0f;
   float finishElapsed_ = 0.0f;
   bool sceneChangeRequested_ = false;
   bool restartRequested_ = false;
   bool runtimeInitialized_ = false;
   bool playerLocked_ = false;
   bool vehicleControllerEnabledWhenUnlocked_ = false;
   bool gravityBodyEnabledWhenUnlocked_ = false;
   bool cameraSwitcherEnabledWhenUnlocked_ = false;
   bool speedPostEffectEnabledWhenRacing_ = false;
   VehicleController* vehicleController_ = nullptr;
   GravityBody* gravityBody_ = nullptr;
   CameraModeSwitcher* cameraSwitcher_ = nullptr;
   VehicleSpeedPostEffectController* speedPostEffectController_ = nullptr;
};

} // namespace App
