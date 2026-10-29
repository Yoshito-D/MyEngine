#pragma once

#include "GameEngine/Object/Component/Base/IObjectComponent.h"
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

   /// @copydoc GameEngine::IObjectComponent::GetTypeName
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief 対象プレイヤー・惑星・ゴール・UIを解決し、最初の説明から開始する。
   void OnSceneLoaded(GameEngine::SceneWorld& sceneWorld) override;

   /// @copydoc GameEngine::IObjectComponent::OnReferencesChanged
   void OnReferencesChanged(GameEngine::SceneWorld& sceneWorld) override;

   /// @brief 決定入力と練習の達成状態を監視して進行する。
   void Update(float deltaTime) override;

   /// @copydoc GameEngine::IObjectComponent::Serialize
   nlohmann::json Serialize() const override;

   /// @copydoc GameEngine::IObjectComponent::Deserialize
   void Deserialize(const nlohmann::json& data) override;

   /// @copydoc GameEngine::ParameterComponent::GetSharedParameters
   nlohmann::json GetSharedParameters() const override;

   /// @copydoc GameEngine::ParameterComponent::ApplySharedParameters
   void ApplySharedParameters(const nlohmann::json& data) override;

#ifdef USE_IMGUI
   /// @brief 参照設定と課題の進捗を表示する。
   void DrawInspector() override;
#endif

private:
   enum class Phase {
      DrivingIntro,
      DriftIntro,
      DriftPractice,
      JumpIntro,
      JumpPractice,
      AirControlIntro,
      PlanetTransfer,
      Goal,
      Complete,
   };

   enum class GuideFadeState {
      Visible,
      FadingOut,
      FadingIn,
   };

   void ReadParameters(const nlohmann::json& data);
   bool HasRequiredReferences() const;
   bool IsOnTargetPlanet() const;
   void SetPhase(Phase phase);
   void SetPlayerPaused(bool paused);
   void SetTargetPlanetEnabled(bool enabled);
   void SetGoalGuidanceEnabled(bool enabled);
   bool RevealTargetPlanet();
   void UpdateGuideText(float deltaTime);
   void ApplyGuideOpacity();
   void UpdateFeedback(float deltaTime);
   void HandleLandingResult();
   std::string BuildGuideText() const;
   const char* GetPhaseName() const;

   static constexpr std::int32_t kRequiredMiniTurbos = 3;         ///< ドリフト課題で発動するミニターボの回数。
   std::string playerObjectId_ = "Model:Player";                  ///< 操作対象の安定Entity ID。
   std::string targetPlanetObjectId_ = "Model:Planet_2";          ///< 練習開始時に出現させる惑星。
   std::string goalObjectId_ = "Model:Goal";                      ///< 目標惑星の地表へ配置するゴール。
   std::string feedbackObjectId_ = "UIText:TutorialFeedback";     ///< 操作結果を即座に表示するUI。
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
   RaceGoalDistanceTextComponent* goalDistance_ = nullptr;  ///< 距離誘導の表示制御。
   RaceGoalDirectionHUDComponent* goalDirection_ = nullptr; ///< 方向誘導の表示制御。

   Phase phase_ = Phase::DrivingIntro;                       ///< 現在の課題。
   GuideFadeState guideFadeState_ = GuideFadeState::Visible; ///< 案内文のフェード状態。
   std::string displayedText_;                               ///< 最後に反映した案内。
   std::string pendingText_;                                 ///< フェード後に反映する案内。
   std::string landingFeedback_;                             ///< 着地の結果に対応する説明。
   std::string driftFeedback_;                               ///< 直近のドリフト結果。
   float driftFeedbackRemaining_ = 0.0f;                     ///< 操作結果を表示する残り時間。進行判定には使わない。
   float guideBaseOpacity_ = 1.0f;                           ///< シーンに保存された案内文の不透明度。
   float guideVisibility_ = 1.0f;                            ///< フェードによる表示割合。
   float completionElapsed_ = 0.0f;                          ///< クリア状態に入ってからの経過時間（秒）。
   std::uint64_t lastMiniTurboFireCount_ = 0;                ///< 前回処理した発動連番。
   std::int32_t miniTurboCount_ = 0;                         ///< この課題内で達成した回数。
   bool wasDrifting_ = false;                                ///< ドリフト終了の検出用。
   bool wasAirborne_ = false;                                ///< 離陸・着地の検出用。
   bool jumpStarted_ = false;                                ///< ジャンプ課題開始後に離陸したか。
   bool usedAirControl_ = false;                             ///< 惑星移動中に姿勢操作を体験したか。
   bool controllerEnabledWhenPlaying_ = true;                ///< シーン読込時の走行制御の有効状態。
   bool gravityEnabledWhenPlaying_ = true;                   ///< シーン読込時の物理の有効状態。
   bool sceneChangeRequested_ = false;                       ///< 二重遷移の抑止。
   bool runtimeInitialized_ = false;                         ///< 編集時の読み込みと実行時の操作停止を分ける。
};

} // namespace App
