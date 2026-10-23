#pragma once

#include "GameEngine/Object/Component/Base/IObjectComponent.h"
#include "GameEngine/Math/Types/Vector3.h"
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace GameEngine {
class Object;
class UITextComponent;
}

namespace App {

/// @brief メニュー項目の参照と基準姿勢を保持し、選択・決定・非表示の演出を管理する。
class MenuSelectionAnimationComponent final : public GameEngine::IObjectComponent {
public:
   static constexpr const char* kTypeName = "MenuSelectionAnimationComponent";
   static constexpr GameEngine::ComponentDisplayName kDisplayName{"メニュー選択演出", "Menu Selection Animation"};

   /// @copydoc GameEngine::IObjectComponent::GetTypeName
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief シーンに保存されたスケールと不透明度を取り直し、入力待ちまで項目を非表示にする。
   void OnSceneLoaded(GameEngine::SceneWorld& sceneWorld) override;

   /// @brief 未解決または変更された項目の参照と基準姿勢を取得する。
   void OnReferencesChanged(GameEngine::SceneWorld& sceneWorld) override;

   /// @brief 選択中の脈動、選択切替、決定時の拡大・フェードを更新する。
   void Update(float deltaTime) override;

   /// @brief 選択項目を変更する。同じ項目への通知では再生位置を維持する。
   /// @param optionIndex 保存された項目一覧の添字。
   /// @param animateSwitch 縮小状態からの切替演出を再生する場合はtrue。
   /// @return TextとTransformを持つ有効な項目を選択できた場合はtrue。
   bool Select(std::int32_t optionIndex, bool animateSwitch = true);

   /// @brief 選択中の実際のスケールから決定演出を開始する。
   /// @return 有効な選択項目があり、決定演出を開始できた場合はtrue。
   bool ConfirmSelection();

   /// @brief 全項目の文字列を消し、スケールと不透明度を基準値へ戻す。
   void Hide();

   /// @brief 基準姿勢を取得済みで、現在もTextとTransformを持つ項目か判定する。
   bool IsOptionAvailable(std::size_t optionIndex) const;

   /// @brief 項目の表示内容を画面側で設定するためのTextを取得する。
   /// @return 項目が利用できない場合はnullptr。
   GameEngine::UITextComponent* GetOptionText(std::size_t optionIndex) const;

   /// @copydoc GameEngine::IObjectComponent::Serialize
   nlohmann::json Serialize() const override;

   /// @copydoc GameEngine::IObjectComponent::Deserialize
   void Deserialize(const nlohmann::json& data) override;

#ifdef USE_IMGUI
   /// @copydoc GameEngine::IObjectComponent::DrawInspector
   void DrawInspector() override;
#endif

private:
   enum class State {
      Hidden,
      Selecting,
      Reacting
   };

   struct OptionVisual {
      std::string objectId;
      GameEngine::Vector3 baseScale = {1.0f, 1.0f, 1.0f};
      float baseOpacity = 1.0f;
      bool captured = false;
   };

   GameEngine::Object* FindOptionObject(std::size_t optionIndex) const;
   float EvaluateSelectionScale(float deltaTime);
   void ApplySelection(float scale);
   void ApplyReaction();
   void ValidateSettings();

   std::vector<OptionVisual> options_;
   float pulseDuration_ = 0.9f;
   float pulseScale_ = 1.1f;
   float switchDuration_ = 0.16f;
   float switchScale_ = 0.82f;
   float reactionDuration_ = 0.4f;
   float reactionEndScale_ = 1.2f;
   State state_ = State::Hidden;
   std::int32_t selectedOption_ = -1;
   float elapsed_ = 0.0f;
   bool switching_ = false;
   bool skipAdvance_ = false;
   GameEngine::Vector3 reactionStartScale_ = {1.0f, 1.0f, 1.0f};
};

} // namespace App
