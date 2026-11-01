#pragma once
#include "GameProject/App/Component/UI/MenuNavigation.h"

#include "GameEngine/Object/Component/Base/IObjectComponent.h"
#include <cstdint>
#include <string>

namespace App {
class MenuSelectionAnimationComponent;

/// @brief タイトル画面のメインメニュー、開始経路選択、終了を制御する。
/// @details 同じObjectのMenuSelectionAnimationComponentの2項目を、メニュー階層ごとに切り替える。
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
   enum class MenuPage {
      Main,
      StartSelection
   };

   enum class PendingAction {
      None,
      OpenStartMenu,
      Quit
   };

   void SwitchMenu(MenuSelectionAnimationComponent& animation, MenuPage page);
   void RefreshSelectionText(MenuSelectionAnimationComponent& animation);

   std::string tutorialScene_ = "Tutorial";            ///< チュートリアル開始時に読み込む登録シーン名。
   std::string stageScene_ = "GameTest";               ///< 通常プレイ開始時に読み込む登録シーン名。
   MenuPage menuPage_ = MenuPage::Main;                ///< 現在表示しているメニュー階層。
   PendingAction pendingAction_ = PendingAction::None; ///< 決定演出の完了後に実行する操作。
   MenuNavigation navigation_;                         ///< 共通の選択位置と入力抑制。
   bool startRequested_ = false;                       ///< シーン遷移または終了を要求済みかどうか。
};

} // namespace App
