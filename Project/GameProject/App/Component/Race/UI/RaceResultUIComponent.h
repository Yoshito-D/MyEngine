#pragma once
#include "GameProject/App/Component/UI/MenuNavigation.h"

#include "GameEngine/Object/Component/Base/IObjectComponent.h"
#include <cstdint>
#include <string>

namespace App {
class MenuSelectionAnimationComponent;
class RaceManagerComponent;

/// @brief ゴールタイムとベスト3を表示し、タイトル・リスタートの選択を処理する。
/// @details 同じObjectのMenuSelectionAnimationComponentで、項目0をタイトル、項目1をリスタートに対応させる。
class RaceResultUIComponent final : public GameEngine::IObjectComponent {
public:
   static constexpr const char* kTypeName = "RaceResultUIComponent"; ///< コンポーネント型名。
   /// @brief エディターに表示する日本語名と英語名。
   static constexpr GameEngine::ComponentDisplayName kDisplayName{"レース結果UI", "Race Result UI"};

   /// @brief コンポーネント型名を取得する。
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief RaceManager参照を解決し、リザルト選択状態を初期化する。
   void OnSceneLoaded(GameEngine::SceneWorld& sceneWorld) override;

   /// @brief 表示対象のRaceManagerをIDから解決する。
   void OnReferencesChanged(GameEngine::SceneWorld& sceneWorld) override;

   /// @brief 結果表示、選択移動、決定入力を処理して演出を通知する。
   void Update(float deltaTime) override;

   /// @brief RaceManager参照とタイトルへの遷移先をJSONへ保存する。
   nlohmann::json Serialize() const override;

   /// @brief JSONからRaceManager参照とタイトルへの遷移先を読み込む。
   void Deserialize(const nlohmann::json& data) override;

   /// @brief シーン固有の参照と実行履歴を除く、共有可能な設定JSONを返す。
   nlohmann::json GetSharedParameters() const override;

   /// @brief 実行履歴を維持したまま共有設定を適用する。
   /// @param data 適用する共有設定JSON。
   void ApplySharedParameters(const nlohmann::json& data) override;

#ifdef USE_IMGUI
   /// @brief 参照状態をインスペクターへ表示する。
   void DrawInspector() override;
#endif

private:
   void ReadParameters(const nlohmann::json& data, bool initializeRuntime);

   void RefreshSelectionText(MenuSelectionAnimationComponent& animation);
   bool ConfirmSelection(MenuSelectionAnimationComponent& animation);
   std::string BuildResultText() const;

   std::string raceManagerId_;                   ///< レース進行を参照する管理Objectの安定Entity ID。
   std::string titleScene_ = "Title";            ///< タイトルへ戻るときに読み込む登録シーン名。
   RaceManagerComponent* raceManager_ = nullptr; ///< 参照解決済みレース管理コンポーネントへの非所有参照。
   MenuNavigation navigation_;                   ///< 共通の選択位置と入力抑制。
   bool resultRequested_ = false;                ///< 結果表示への切り替えを要求済みかどうか。
   bool showingResult_ = false;                  ///< 現在レース結果のメニューを表示しているかどうか。
};

} // namespace App
