#pragma once

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
   static constexpr const char* kTypeName = "RaceResultUIComponent";
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

#ifdef USE_IMGUI
   /// @brief 参照状態をインスペクターへ表示する。
   void DrawInspector() override;
#endif

private:
   bool MoveSelection(MenuSelectionAnimationComponent& animation, std::int32_t direction);
   void RefreshSelectionText(MenuSelectionAnimationComponent& animation);
   bool ConfirmSelection(MenuSelectionAnimationComponent& animation);
   std::string BuildResultText() const;

   std::string raceManagerId_;
   std::string titleScene_ = "Title";
   RaceManagerComponent* raceManager_ = nullptr;
   std::int32_t selectedOption_ = 1;
   bool resultRequested_ = false;
   bool navigationLatched_ = false;
   bool showingResult_ = false;
};

} // namespace App
