#pragma once

#include "GameEngine/Object/Component/Base/IObjectComponent.h"
#include <cstdint>
#include <string>

namespace App {
class MenuSelectionAnimationComponent;

/// @brief タイトル画面の開始経路選択と画面遷移を制御する。
/// @details 同じObjectのMenuSelectionAnimationComponentで、項目0をチュートリアル、項目1をステージに対応させる。
class TitleStartComponent final : public GameEngine::IObjectComponent {
public:
   static constexpr const char* kTypeName = "TitleStartComponent";
   static constexpr GameEngine::ComponentDisplayName kDisplayName{"タイトル開始UI", "Title Start UI"};

   /// @brief コンポーネント型名を取得する。
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief 選択と入力ラッチを初期化する。UI参照と演出は専用コンポーネントが管理する。
   void OnSceneLoaded(GameEngine::SceneWorld& sceneWorld) override;

   /// @brief 選択・決定入力を処理し、演出を通知してシーン遷移を要求する。
   void Update(float deltaTime) override;

   /// @brief 選択肢の遷移先をJSONへ保存する。
   nlohmann::json Serialize() const override;

   /// @brief JSONから選択肢の遷移先を読み込む。
   void Deserialize(const nlohmann::json& data) override;

#ifdef USE_IMGUI
   /// @brief タイトル開始設定をインスペクターへ表示する。
   void DrawInspector() override;
#endif

private:
   bool MoveSelection(MenuSelectionAnimationComponent& animation, std::int32_t direction);
   void RefreshSelectionText(MenuSelectionAnimationComponent& animation);

   std::string tutorialScene_ = "Tutorial";
   std::string stageScene_ = "GameTest";
   std::int32_t selectedOption_ = -1;
   bool startRequested_ = false;
   bool navigationLatched_ = false;
};

} // namespace App
