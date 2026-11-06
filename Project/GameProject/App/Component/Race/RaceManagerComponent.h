#pragma once
#include "GameProject/App/Component/Race/RacePresentation.h"
#include "GameProject/App/Component/Race/RaceRecordStore.h"

#include "GameEngine/Object/Component/Base/IObjectComponent.h"
#include "GameEngine/Utility/StateMachine.h"
#include "GameEngine/Scene/Camera/Core/CinemachineBrain.h"
#include <cstddef>
#include <string>
#include <vector>

namespace App {

class CameraModeSwitcher;
class GravityBody;
class VehicleController;
class VehicleInputComponent;
class VehicleSpeedPostEffectController;

/// @brief タイムアタックの状態、経過時間、チェックポイント進行を管理する
class RaceManagerComponent final : public GameEngine::IObjectComponent {
public:
   /// @brief 各進行状態の入場・更新・退場処理と遷移規則を登録する。
   RaceManagerComponent();

   /// @brief レース進行状態
   enum class State {
      Waiting,
      Intro,
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
   static constexpr size_t kBestTimeCount = RaceRecordStore::kBestTimeCount;

   /// @brief コンポーネント型名を取得する
   /// @return RaceManagerComponent
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief レース状態と計測時間を更新する
   /// @param deltaTime ゲーム用デルタタイム（秒）
   void Update(float deltaTime) override;

   /// @brief シーン内のプレイヤー参照と記録を復元し、開始状態へリセットする
   /// @param sceneWorld 所属するシーンワールド
   void OnSceneLoaded(GameEngine::SceneWorld& sceneWorld) override;

   /// @brief 参照設定の変更後に、指定ワールドから依存先を解決し直す。
   /// @param sceneWorld 参照先を検索するシーン。
   void OnReferencesChanged(GameEngine::SceneWorld& sceneWorld) override;

   /// @brief 一時演出を中止し、生存する対象の操作・カメラ・暗転を解除する。
   void OnDisable() override;

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
   State GetState() const { return stateMachine_.GetCurrentState().value_or(previewState_); }

   /// @brief 現在の計測時間を取得する
   /// @return 秒単位の経過時間
   float GetElapsedTime() const { return elapsedTime_; }

   /// @brief 現在のベストタイムを取得する
   /// @return 記録がない場合は0、記録がある場合は秒単位のタイム
   float GetBestTime() const { return records_.GetBestTimes().empty() ? 0.0f : records_.GetBestTimes().front(); }

   /// @brief 保存済みベストタイムを速い順で取得する
   /// @return 最大3件の秒単位タイム
   const std::vector<float>& GetBestTimes() const { return records_.GetBestTimes(); }

   /// @brief カウントダウン残り時間を取得する
   /// @return 秒単位の残り時間
   float GetCountdownRemaining() const { return countdownRemaining_; }

   /// @brief START表示期間中か調べる
   /// @return STARTを表示する場合はtrue
   bool IsStartBannerVisible() const { return GetState() == State::Running && startBannerRemaining_ > 0.0f; }

   /// @brief 設定されたレースシーンの完全な再読み込みを要求する
   /// @return シーン遷移要求を受け付けた場合はtrue
   bool RequestRestart();

   /// @brief レース設定をJSONへ保存する
   /// @return 保存用JSON
   nlohmann::json Serialize() const override;

   /// @brief JSONからレース設定を読み込む
   /// @param data レース設定JSON
   void Deserialize(const nlohmann::json& data) override;

   /// @brief シーン固有の参照と実行履歴を除く、共有可能な設定JSONを返す。
   nlohmann::json GetSharedParameters() const override;

   /// @brief 実行履歴を維持したまま共有設定を適用する。
   /// @param data 適用する共有設定JSON。
   void ApplySharedParameters(const nlohmann::json& data) override;

#ifdef USE_IMGUI

   /// @brief レース状態と設定をインスペクターへ表示する
   void DrawInspector() override;
#endif

private:
   void ReadParameters(const nlohmann::json& data, bool initializeRuntime);

   void ConfigureStates();
   void EnterCountdown();
   void EnterRunning();
   void EnterFinished();
   void UpdateIntro(float deltaTime);
   void UpdateFinished(float deltaTime);
   bool CanFinish() const;

#ifdef USE_IMGUI
   void DrawIntroInspector();
#endif

   GameEngine::StateMachine<State> stateMachine_;  ///< レース進行と入場・退場処理を管理する。
   State previewState_ = State::Waiting;           ///< 実行開始前と無効化後に表示する進行状態。
   StartMode startMode_ = StartMode::Countdown;    ///< 即時・カウントダウン・ゲート通過の開始方式。
   float elapsedTime_ = 0.0f;                      ///< 現在の計測時間（秒）。
   RaceRecordStore records_;                       ///< レース記録の所有者。
   float countdownSeconds_ = 3.0f;                 ///< 開始前カウントダウンの時間（秒）。
   float countdownRemaining_ = 3.0f;               ///< カウントダウン残り時間（秒）。
   bool useCountdownCameraPose_ = false;           ///< カウントダウン専用の保存姿勢を使用するか。
   GameEngine::Vector3 countdownCameraPosition_{}; ///< カウントダウン中のワールド位置。
   GameEngine::Vector3 countdownCameraRotation_{}; ///< カウントダウン中のEuler角（ラジアン）。
   float countdownCameraFov_ = 0.45f;              ///< カウントダウン中の垂直画角（ラジアン）。
   float startCameraBlendSeconds_ = 1.0f;          ///< 開始時に走行カメラへ移行する時間（秒）。
   /// @brief 開始時の補間曲線。既定では速く動き始めて徐々に減速する。
   GameEngine::CinemachineBrain::BlendEasing startCameraBlendEasing_ =
      GameEngine::CinemachineBrain::BlendEasing::EaseOut;
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
   bool restartAfterUpdate_ = false;                    ///< 状態更新中に要求された再開始をコールバック終了後に行うか。
   bool showStartBannerOnEntry_ = true;                 ///< Runningへの入場時にSTART表示を開始するか。
   StageIntroSettings introSettings_;                   ///< ステージ固有の俯瞰構図と再生時間。
   bool introSkipArmed_ = false;                        ///< 導入開始後に決定入力の解放を確認したか。
   RacePresentation presentation_;                      ///< 操作停止とカメラ演出の所有者。
};

} // namespace App
