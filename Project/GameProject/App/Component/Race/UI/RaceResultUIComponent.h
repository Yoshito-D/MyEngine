#pragma once

#include "GameEngine/Object/Component/Base/IObjectComponent.h"
#include "GameEngine/Math/VectorMath.h"
#include <array>
#include <cstddef>
#include <string>

namespace GameEngine {
class TransformComponent;
class UITextComponent;
}

namespace App {

class RaceManagerComponent;

/// @brief ゴールタイム、ベスト3、タイトル・リスタートの選択UIを表示する
class RaceResultUIComponent final : public GameEngine::IObjectComponent {
public:
   static constexpr const char* kTypeName = "RaceResultUIComponent";
   static constexpr GameEngine::ComponentDisplayName kDisplayName{ "レース結果UI", "Race Result UI" };

   /// @brief コンポーネント型名を取得する
   /// @return RaceResultUIComponent
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief UI参照を解決し、リザルト選択状態を初期化する
   /// @param sceneWorld 所属するシーンワールド
   void OnSceneLoaded(GameEngine::SceneWorld& sceneWorld) override;

   /// @brief 表示対象のRaceManagerをIDから解決する
   /// @param sceneWorld 所属するシーンワールド
   void OnReferencesChanged(GameEngine::SceneWorld& sceneWorld) override;

   /// @brief 結果表示、選択移動、決定リアクションを更新する
   /// @param deltaTime ゲーム用デルタタイム（秒）
   void Update(float deltaTime) override;

   /// @brief RaceManager参照をJSONへ保存する
   /// @return 保存用JSON
   nlohmann::json Serialize() const override;

   /// @brief JSONからRaceManager参照を読み込む
   /// @param data 表示設定JSON
   void Deserialize(const nlohmann::json& data) override;

#ifdef USE_IMGUI
   /// @brief 参照状態をインスペクターへ表示する
   void DrawInspector() override;
#endif

private:
   static constexpr std::size_t kOptionCount = 2;

   void ResolveOptionVisuals(GameEngine::SceneWorld& sceneWorld);
   bool CaptureBaseVisualStates();
   bool IsOptionAvailable(std::size_t optionIndex) const;
   bool MoveSelection(int direction);
   void RefreshSelectionText();
   void UpdateSelectionAnimation(float deltaTime);
   void HideOptionVisuals();
   void ApplyResultReaction(std::size_t optionIndex);
   bool ConfirmSelection();
   std::string BuildResultText() const;

   std::string raceManagerId_;
   std::string titleOptionObjectId_ = "UIText:ResultTitlePrompt";
   std::string restartOptionObjectId_ = "UIText:ResultRestartPrompt";
   std::string titleScene_ = "Title";
   float reactionDuration_ = 0.4f;
   float reactionEndScale_ = 1.2f;
   float selectionPulseDuration_ = 1.8f;
   float selectionPulseScale_ = 1.1f;
   float selectionSwitchDuration_ = 0.16f;
   float selectionSwitchScale_ = 0.82f;
   float reactionElapsed_ = 0.0f;
   float selectionAnimationElapsed_ = 0.0f;
   float selectionSwitchElapsed_ = 0.0f;
   GameEngine::Vector3 reactionStartScale_ = { 1.0f, 1.0f, 1.0f };
   std::array<GameEngine::UITextComponent*, kOptionCount> optionTexts_ = {};
   std::array<GameEngine::TransformComponent*, kOptionCount> optionTransforms_ = {};
   std::array<GameEngine::Vector3, kOptionCount> baseScales_ = {
      GameEngine::Vector3{ 1.0f, 1.0f, 1.0f },
      GameEngine::Vector3{ 1.0f, 1.0f, 1.0f }
   };
   std::array<float, kOptionCount> baseOpacities_ = { 1.0f, 1.0f };
   RaceManagerComponent* raceManager_ = nullptr;
   int selectedOption_ = 1;
   bool resultRequested_ = false;
   bool navigationLatched_ = false;
   bool showingResult_ = false;
   bool hasBaseVisualStates_ = false;
   bool selectionSwitchActive_ = false;
};

} // namespace App
