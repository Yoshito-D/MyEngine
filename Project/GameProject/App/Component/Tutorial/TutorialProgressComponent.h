#pragma once

#include "Object/Component/IObjectComponent.h"
#include <string>

namespace GameEngine {
class SceneWorld;
class UITextComponent;
}

namespace App {

class CharacterJump;
class CharacterLanding;
class PlanetSwitcher;
class VehicleLandingBoost;

/// @brief 基本操作から惑星間ジャンプと成功着地までを段階的に案内する
class TutorialProgressComponent final : public GameEngine::IObjectComponent {
public:
   static constexpr const char* kTypeName = "TutorialProgressComponent";
   static constexpr GameEngine::ComponentDisplayName kDisplayName{
      "チュートリアル進行",
      "Tutorial Progress"
   };

   /// @brief コンポーネント型名を取得する
   /// @return TutorialProgressComponent
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief チュートリアル対象のプレイヤー参照を解決して進行を初期化する
   /// @param sceneWorld 所属するシーンワールド
   void OnSceneLoaded(GameEngine::SceneWorld& sceneWorld) override;

   /// @brief 操作・惑星切替・着地結果を監視してチュートリアルを進める
   /// @param deltaTime ゲーム用デルタタイム（秒）
   void Update(float deltaTime) override;

   /// @brief 対象プレイヤー、目標惑星、完了後遷移をJSONへ保存する
   /// @return 保存用JSON
   nlohmann::json Serialize() const override;

   /// @brief JSONから対象プレイヤー、目標惑星、完了後遷移を読み込む
   /// @param data 設定JSON
   void Deserialize(const nlohmann::json& data) override;

#ifdef USE_IMGUI
   /// @brief チュートリアル設定と現在の進行状態をインスペクターへ表示する
   void DrawInspector() override;
#endif

private:
   enum class Phase {
      Steering,
      Jump,
      AirControl,
      PlanetTransfer,
      LandingPractice,
      Complete,
   };

   void SetPhase(Phase phase);
   void UpdateGuideText();
   void HandleLandingResult();
   std::string BuildGuideText() const;
   const char* GetPhaseName() const;

   std::string playerObjectId_ = "Model:Player";
   std::string nextScene_ = "GameTest";
   int targetPlanetIndex_ = 1;
   float completionDelay_ = 2.5f;
   float completionElapsed_ = 0.0f;
   Phase phase_ = Phase::Steering;
   CharacterJump* characterJump_ = nullptr;
   CharacterLanding* characterLanding_ = nullptr;
   PlanetSwitcher* planetSwitcher_ = nullptr;
   VehicleLandingBoost* landingBoost_ = nullptr;
   GameEngine::UITextComponent* guideText_ = nullptr;
   std::string displayedText_;
   std::string landingFeedback_;
   bool usedPitch_ = false;
   bool usedRoll_ = false;
   bool wasGrounded_ = true;
   bool sceneChangeRequested_ = false;
};

} // namespace App
