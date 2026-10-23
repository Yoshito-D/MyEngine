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
   static constexpr const char* kTypeName = "TitleStartComponent"; ///< コンポーネント型名。
   /// @brief エディターに表示する日本語名と英語名。
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

   std::string tutorialScene_ = "Tutorial"; ///< チュートリアル開始時に読み込む登録シーン名。
   std::string stageScene_ = "GameTest";    ///< 通常プレイ開始時に読み込む登録シーン名。
   std::int32_t selectedOption_ = -1;       ///< 現在選択中のメニュー項目番号。負値は未選択。
   bool startRequested_ = false;            ///< メニュー選択によるプレイ開始を要求済みかどうか。
   bool navigationLatched_ = false;         ///< 方向入力が中立に戻るまで次のメニュー移動を抑止する状態。
};

} // namespace App
