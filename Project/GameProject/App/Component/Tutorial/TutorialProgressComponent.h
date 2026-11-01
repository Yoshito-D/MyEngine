#pragma once
#include "GameProject/App/Component/Tutorial/TutorialGuidePresenter.h"

#include "GameEngine/Object/Component/Base/IObjectComponent.h"
#include "GameEngine/Math/Types/Vector3.h"
#include <cstdint>
#include <string>

namespace GameEngine {
class Object;
class SceneWorld;
class UITextComponent;
}

namespace App {

class CharacterJump;
class CharacterLanding;
class GravityBody;
class PlanetSwitcher;
class RaceGoalDirectionHUDComponent;
class RaceGoalDistanceTextComponent;
class VehicleController;
class VehicleDrift;
class VehicleInputComponent;
class VehicleLandingBoost;

/// @brief 決定ボタンで説明を進め、ドリフト・ジャンプ・惑星移動からゴールまでを案内する。
class TutorialProgressComponent final : public GameEngine::IObjectComponent {
public:
   static constexpr const char* kTypeName = "TutorialProgressComponent"; ///< コンポーネント型名。
   static constexpr GameEngine::ComponentDisplayName kDisplayName{
      "チュートリアル進行", "Tutorial Progress"}; ///< 表示名。

   /// @brief シーン保存とコンポーネント検索に使う型名を返す。
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief 対象プレイヤー・惑星・ゴール・UIを解決し、最初の説明から開始する。
   void OnSceneLoaded(GameEngine::SceneWorld& sceneWorld) override;

   /// @brief 参照設定の変更後に、指定ワールドから依存先を解決し直す。
   /// @param sceneWorld 参照先を検索するシーン。
   void OnReferencesChanged(GameEngine::SceneWorld& sceneWorld) override;

   /// @brief 決定入力と練習の達成状態を監視して進行する。
   void Update(float deltaTime) override;

   /// @brief コンポーネントの保存対象設定をJSONで返す。
   nlohmann::json Serialize() const override;

   /// @brief JSONから設定を復元する。
   /// @param data 保存されたコンポーネント設定。
   void Deserialize(const nlohmann::json& data) override;

   /// @brief シーン固有の参照と実行履歴を除く、共有可能な設定JSONを返す。
   nlohmann::json GetSharedParameters() const override;

   /// @brief 実行履歴を維持したまま共有設定を適用する。
   /// @param data 適用する共有設定JSON。
   void ApplySharedParameters(const nlohmann::json& data) override;

#ifdef USE_IMGUI
   /// @brief 参照設定と課題の進捗を表示する。
   void DrawInspector() override;
#endif

private:
   using Phase = TutorialPhase;

   void ReadParameters(const nlohmann::json& data);
   bool HasRequiredReferences() const;
   bool IsGamepadConnected() const;
   bool IsOnTargetPlanet() const;
   void SetPhase(Phase phase);
   void UpdatePresentation(float deltaTime);
   void SetPlayerPaused(bool paused);
   void SetTargetPlanetEnabled(bool enabled);
   void SetGoalGuidanceEnabled(bool enabled);
   bool RevealTargetPlanet();
   const char* GetPhaseName() const;

   static constexpr std::int32_t kRequiredMiniTurbos = 3;         ///< ドリフト課題で発動するミニターボの回数。
   std::string playerObjectId_ = "Model:Player";                  ///< 操作対象の安定Entity ID。
   std::string targetPlanetObjectId_ = "Model:Planet_2";          ///< 練習開始時に出現させる惑星。
   std::string goalObjectId_ = "Model:Goal";                      ///< 目標惑星の地表へ配置するゴール。
   std::string feedbackObjectId_ = "UIText:TutorialFeedback";     ///< 操作結果を即座に表示するUI。
   std::string promptObjectId_ = "UIText:TutorialPrompt";         ///< 本文より小さく表示する決定操作のUI。
   std::string goalDistanceObjectId_ = "UIText:GoalDistanceText"; ///< ゴール距離のUI。
   std::string goalDirectionObjectId_ = "Model:GoalDirectionHUD"; ///< ゴール方向のUI。
   std::string nextScene_ = "GameTest";                           ///< クリア表示後の自動遷移先。
   float completionDelay_ = 2.5f;                                 ///< クリア表示から自動遷移までの待機時間（秒）。
   float guideFadeDuration_ = 0.2f;                               ///< 課題の案内を切り替えるフェード時間（秒）。
   float transferSurfaceGap_ = 50.0f; ///< 出現時のプレイヤーから目標惑星の手前側地表までの距離。

   GameEngine::Object* playerObject_ = nullptr;             ///< プレイヤーへの非所有参照。
   GameEngine::Object* targetPlanet_ = nullptr;             ///< 目標惑星への非所有参照。
   GameEngine::Object* goalObject_ = nullptr;               ///< ゴールへの非所有参照。
   CharacterJump* characterJump_ = nullptr;                 ///< 離陸の状態。
   CharacterLanding* characterLanding_ = nullptr;           ///< 接地の状態。
   GravityBody* gravityBody_ = nullptr;                     ///< クリア時の物理停止と地表法線の参照。
   PlanetSwitcher* planetSwitcher_ = nullptr;               ///< 着地先の参照。
   VehicleController* vehicleController_ = nullptr;         ///< クリア時に停止する走行制御。
   VehicleInputComponent* vehicleInput_ = nullptr;          ///< 対象プレイヤーの入力。
   VehicleDrift* vehicleDrift_ = nullptr;                   ///< ミニターボ発動の実績。
   VehicleLandingBoost* landingBoost_ = nullptr;            ///< 着地品質の結果。
   GameEngine::UITextComponent* guideText_ = nullptr;       ///< 現在の課題と説明文。
   GameEngine::UITextComponent* feedbackText_ = nullptr;    ///< 練習の即時フィードバック。
   GameEngine::UITextComponent* promptText_ = nullptr;      ///< 次へ進むための操作案内。
   RaceGoalDistanceTextComponent* goalDistance_ = nullptr;  ///< 距離誘導の表示制御。
   RaceGoalDirectionHUDComponent* goalDirection_ = nullptr; ///< 方向誘導の表示制御。

   TutorialGuidePresenter guidePresenter_;    ///< 案内文面とフェードの所有者。
   Phase phase_ = Phase::DrivingIntro;        ///< 現在の課題。
   float completionElapsed_ = 0.0f;           ///< クリア状態に入ってからの経過時間（秒）。
   std::uint64_t lastMiniTurboFireCount_ = 0; ///< 前回処理した発動連番。
   std::int32_t miniTurboCount_ = 0;          ///< この課題内で達成した回数。
   bool wasDrifting_ = false;                 ///< ドリフト終了の検出用。
   bool wasAirborne_ = false;                 ///< 離陸・着地の検出用。
   bool jumpStarted_ = false;                 ///< ジャンプ課題開始後に離陸したか。
   GameEngine::Vector3 airControlStartUp_{};   ///< 練習ジャンプの離陸時の機体上方向。
   bool airControlInputReceived_ = false;     ///< 今回のジャンプ中に姿勢操作を入力したか。
   bool airRotationPerformed_ = false;        ///< 今回のジャンプ中に機体を十分に傾けたか。
   bool controllerEnabledWhenPlaying_ = true; ///< シーン読込時の走行制御の有効状態。
   bool gravityEnabledWhenPlaying_ = true;    ///< シーン読込時の物理の有効状態。
   bool sceneChangeRequested_ = false;        ///< 二重遷移の抑止。
   bool runtimeInitialized_ = false;          ///< 編集時の読み込みと実行時の操作停止を分ける。
};

} // namespace App
