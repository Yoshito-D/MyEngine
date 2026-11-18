#pragma once
#include "GameProject/App/Component/UI/MenuNavigation.h"

#include "GameEngine/Object/Component/Base/IObjectComponent.h"
#include "GameEngine/Utility/StateMachine.h"
#include <cstdint>
#include <optional>
#include <span>
#include <string>

namespace GameEngine {
class UITextComponent;
}

namespace App {
class MenuSelectionAnimationComponent;

/// @brief タイトル画面のメインメニュー、開始経路選択、音量設定、終了を制御する。
class TitleStartComponent final : public GameEngine::IObjectComponent {
public:
   /// @brief メニュー操作・決定演出待ち・遷移要求の状態と遷移規則を登録する。
   TitleStartComponent();

   static constexpr const char* kTypeName = "TitleStartComponent"; ///< コンポーネント型名。
   /// @brief エディターに表示する日本語名と英語名。
   static constexpr GameEngine::ComponentDisplayName kDisplayName{"タイトル開始UI", "Title Start UI"};

   /// @brief コンポーネント型名を取得する。
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief 選択と入力ラッチを初期化する。UI参照と演出は専用コンポーネントが管理する。
   void OnSceneLoaded(GameEngine::SceneWorld& sceneWorld) override;

   /// @brief 選択・決定入力を処理し、演出を通知してシーン遷移を要求する。
   void Update(float deltaTime) override;

   /// @brief 設定画面のために隠したタイトル名を元の表示状態に戻す。
   void OnDetach() override;

   /// @brief 選択肢の遷移先とタイトル名の参照をJSONへ保存する。
   nlohmann::json Serialize() const override;

   /// @brief JSONから選択肢の遷移先とタイトル名の参照を読み込む。
   void Deserialize(const nlohmann::json& data) override;

#ifdef USE_IMGUI
   /// @brief タイトル開始設定をインスペクターへ表示する。
   void DrawInspector() override;
#endif

private:
   enum class State {
      Main,
      StartSelection,
      OpeningStartMenu,
      OpeningSettings,
      Settings,
      Quitting,
      StartingScene,
      Exiting
   };

   enum class MenuInput {
      None,
      Confirm,
      Cancel
   };

   void ConfigureStates();
   void UpdateMainMenu();
   void UpdateStartSelection();
   void OpenSettings();
   MenuInput ReadMenuInput(MenuSelectionAnimationComponent& animation);
   bool ConfirmSelection(MenuSelectionAnimationComponent& animation);
   void ExitApplication();
   MenuSelectionAnimationComponent* GetAnimation();
   GameEngine::UITextComponent* GetTitleText();
   void RestoreTitleText();
   std::span<const char* const> GetMenuLabels() const;
   void RevealMenu(MenuSelectionAnimationComponent& animation);
   void RefreshSelectionText(MenuSelectionAnimationComponent& animation);

   std::string tutorialScene_ = "Tutorial";       ///< チュートリアル開始時に読み込む登録シーン名。
   std::string stageScene_ = "GameTest";          ///< 通常プレイ開始時に読み込む登録シーン名。
   std::string titleTextId_;                      ///< 音量設定の表示中に隠すタイトル名のObject ID。
   std::optional<float> titleOpacity_;            ///< 音量設定を開く前のタイトル名の不透明度。
   GameEngine::StateMachine<State> stateMachine_; ///< メニュー階層・演出待ち・要求済み状態を一元管理する。
   MenuNavigation navigation_;                    ///< 共通の選択位置と入力抑制。
};

} // namespace App
