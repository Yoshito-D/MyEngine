#pragma once

#include <cstdint>
#include "GameEngine/Object/Component/Base/IObjectComponent.h"
#include <string>

namespace GameEngine {
class SceneWorld;
class UITextComponent;
}

namespace App {

class CharacterJump;
class CharacterLanding;
class PlanetSwitcher;
class VehicleInputComponent;
class VehicleLandingBoost;

/// @brief 基本操作から惑星間ジャンプと成功着地までを段階的に案内する
class TutorialProgressComponent final : public GameEngine::IObjectComponent {
public:
   static constexpr const char* kTypeName = "TutorialProgressComponent"; ///< コンポーネント型名。
   /// @brief エディターに表示する日本語名と英語名。
   static constexpr GameEngine::ComponentDisplayName kDisplayName{"チュートリアル進行", "Tutorial Progress"};

   /// @brief コンポーネント型名を取得する
   /// @return TutorialProgressComponent
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief チュートリアル対象のプレイヤー参照を解決して進行を初期化する
   /// @param sceneWorld 所属するシーンワールド
   void OnSceneLoaded(GameEngine::SceneWorld& sceneWorld) override;

   /// @copydoc GameEngine::IObjectComponent::OnReferencesChanged
   void OnReferencesChanged(GameEngine::SceneWorld& sceneWorld) override;

   /// @brief 操作・惑星切替・着地結果を監視してチュートリアルを進める
   /// @param deltaTime ゲーム用デルタタイム（秒）
   void Update(float deltaTime) override;

   /// @brief 対象プレイヤー、目標惑星、表示フェード、完了後遷移をJSONへ保存する
   /// @return 保存用JSON
   nlohmann::json Serialize() const override;

   /// @brief JSONから対象プレイヤー、目標惑星、表示フェード、完了後遷移を読み込む
   /// @param data 設定JSON
   void Deserialize(const nlohmann::json& data) override;

   /// @copydoc GameEngine::ParameterComponent::GetSharedParameters
   nlohmann::json GetSharedParameters() const override;

   /// @copydoc GameEngine::ParameterComponent::ApplySharedParameters
   void ApplySharedParameters(const nlohmann::json& data) override;

#ifdef USE_IMGUI

   /// @brief チュートリアル設定と現在の進行状態をインスペクターへ表示する
   void DrawInspector() override;
#endif

private:
   void ReadParameters(const nlohmann::json& data, bool initializeRuntime);

   enum class Phase {
      Steering,
      Jump,
      AirControl,
      PlanetTransfer,
      LandingPractice,
      Complete,
   };

   enum class GuideFadeState {
      Visible,
      FadingOut,
      FadingIn,
   };

   void SetPhase(Phase phase);
   std::int32_t GetNormalizedTargetPlanetIndex() const;
   void NormalizeTargetPlanetIndex();
   void UpdateGuideText(float deltaTime);
   void ApplyGuideOpacity();
   void HandleLandingResult();
   std::string BuildGuideText() const;
   const char* GetPhaseName() const;

   std::string playerObjectId_ = "Model:Player";             ///< 操作対象プレイヤーの安定Entity ID。
   std::string nextScene_ = "GameTest";                      ///< 進行完了後に読み込む登録シーン名。
   std::int32_t targetPlanetIndex_ = 1;                      ///< チュートリアルで到達すべき惑星リンクの番号。
   float completionDelay_ = 2.5f;                            ///< 完了条件成立から次シーンへ進むまでの待機時間（秒）。
   float guideFadeDuration_ = 0.2f;                          ///< 案内文章の切り替え時にフェードする時間（秒）。
   float completionElapsed_ = 0.0f;                          ///< チュートリアル完了後に経過した時間（秒）。
   Phase phase_ = Phase::Steering;                           ///< チュートリアルの現在の進行段階。
   GuideFadeState guideFadeState_ = GuideFadeState::Visible; ///< 案内文章の表示・消去・切り替えの進行状態。
   CharacterJump* characterJump_ = nullptr;        ///< プレイヤーのジャンプ入力と空中状態を扱うコンポーネントへの参照。
   CharacterLanding* characterLanding_ = nullptr;  ///< 着地状態を参照するコンポーネントへの非所有参照。
   PlanetSwitcher* planetSwitcher_ = nullptr;      ///< 現在の惑星と切り替え状況を参照するコンポーネント。
   VehicleInputComponent* vehicleInput_ = nullptr; ///< プレイヤーのアクション入力を参照するコンポーネント。
   VehicleLandingBoost* landingBoost_ = nullptr;   ///< 着地ブーストの成功状態を参照するコンポーネント。
   GameEngine::UITextComponent* guideText_ = nullptr; ///< 案内文章の表示内容と透明度を変更するUIText部品への参照。
   std::string displayedText_;                        ///< 最後にUIへ反映した文字列。重複更新を避けるために保持する。
   std::string pendingText_;                          ///< フェードアウト完了後に表示する次の案内文章。
   std::string landingFeedback_;                      ///< 着地結果に応じて表示する案内文字列。
   float guideBaseOpacity_ = 1.0f;                    ///< 案内UIが保存時に持っていた基準不透明度。
   float guideVisibility_ = 1.0f;                     ///< フェード演出で案内の基準不透明度へ掛ける表示割合。
   bool usedPitch_ = false;                           ///< チュートリアル中にピッチ操作を行ったかどうか。
   bool usedRoll_ = false;                            ///< チュートリアル中にロール操作を行ったかどうか。
   bool wasGrounded_ = true;                          ///< 着地遷移検出用の前フレーム接地状態。
   bool sceneChangeRequested_ = false;                ///< この進行状態からシーン切り替えを要求済みかどうか。
};

} // namespace App
