#pragma once

#include "Object/Component/IObjectComponent.h"
#include "Utility/VectorMath.h"
#include <array>
#include <string>

namespace GameEngine {
class TransformComponent;
class UITextComponent;
}

namespace App {

/// @brief タイトル画面の開始経路選択と決定リアクションを制御する
class TitleStartComponent final : public GameEngine::IObjectComponent {
public:
   static constexpr const char* kTypeName = "TitleStartComponent";
   static constexpr GameEngine::ComponentDisplayName kDisplayName{ "タイトル開始UI", "Title Start UI" };

   /// @brief コンポーネント型名を取得する
   /// @return TitleStartComponent
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief UIの初期表示状態を保存して入力待ちへ戻す
   /// @param sceneWorld 所属するシーンワールド
   void OnSceneLoaded(GameEngine::SceneWorld& sceneWorld) override;

   /// @brief 決定入力を検出し、遷移中の拡大フェードを更新する
   /// @param deltaTime ゲーム用デルタタイム（秒）
   void Update(float deltaTime) override;

   /// @brief 選択肢の遷移先とリアクション設定をJSONへ保存する
   /// @return 保存用JSON
   nlohmann::json Serialize() const override;

   /// @brief JSONから選択肢の遷移先とリアクション設定を読み込む
   /// @param data 設定JSON
   void Deserialize(const nlohmann::json& data) override;

#ifdef USE_IMGUI
   /// @brief タイトル開始設定をインスペクターへ表示する
   void DrawInspector() override;
#endif

private:
   bool ResolveOptionVisuals(GameEngine::SceneWorld& sceneWorld);
   bool CaptureBaseVisualStates();
   void RefreshSelectionText();
   const std::string& GetSelectedSceneName() const;
   void ApplyStartReaction(size_t optionIndex);

   std::string tutorialOptionObjectId_ = "UIText:StartPrompt";
   std::string stageOptionObjectId_ = "UIText:StageStartPrompt";
   std::string tutorialScene_ = "Tutorial";
   std::string stageScene_ = "GameTest";
   float reactionDuration_ = 0.4f;
   float reactionEndScale_ = 1.4f;
   float reactionElapsed_ = 0.0f;
   std::array<GameEngine::UITextComponent*, 2> optionTexts_ = {};
   std::array<GameEngine::TransformComponent*, 2> optionTransforms_ = {};
   std::array<GameEngine::Vector3, 2> baseScales_ = {
      GameEngine::Vector3{ 1.0f, 1.0f, 1.0f },
      GameEngine::Vector3{ 1.0f, 1.0f, 1.0f }
   };
   std::array<float, 2> baseOpacities_ = { 1.0f, 1.0f };
   int selectedOption_ = 0;
   bool startRequested_ = false;
   bool navigationLatched_ = false;
   bool hasBaseVisualStates_ = false;
};

} // namespace App
