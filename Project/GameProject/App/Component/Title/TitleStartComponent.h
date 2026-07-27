#pragma once

#include "Object/Component/IObjectComponent.h"
#include "Utility/VectorMath.h"
#include <string>

namespace GameEngine {
class TransformComponent;
class UITextComponent;
}

namespace App {

/// @brief タイトル画面の決定入力と開始リアクションを制御する
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

   /// @brief 遷移先とリアクション設定をJSONへ保存する
   /// @return 保存用JSON
   nlohmann::json Serialize() const override;

   /// @brief JSONから遷移先とリアクション設定を読み込む
   /// @param data 設定JSON
   void Deserialize(const nlohmann::json& data) override;

#ifdef USE_IMGUI
   /// @brief タイトル開始設定をインスペクターへ表示する
   void DrawInspector() override;
#endif

private:
   bool CaptureBaseVisualState();
   void ApplyStartReaction(
      GameEngine::UITextComponent& text,
      GameEngine::TransformComponent& transform);

   std::string nextScene_ = "GameTest";
   float reactionDuration_ = 0.4f;
   float reactionEndScale_ = 1.4f;
   float reactionElapsed_ = 0.0f;
   GameEngine::Vector3 baseScale_ = { 1.0f, 1.0f, 1.0f };
   float baseOpacity_ = 1.0f;
   bool startRequested_ = false;
   bool hasBaseVisualState_ = false;
};

} // namespace App
