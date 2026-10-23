#pragma once

#include "GameEngine/Object/Component/Base/IObjectComponent.h"
#include <string>

namespace App {

class CharacterJump;
class RaceManagerComponent;

/// @brief ゲームパッドの操作ガイドをリザルト以外でUITextへ表示する
class GamepadGuideTextComponent final : public GameEngine::IObjectComponent {
public:
   /// @brief シーン保存とファクトリ検索で使うコンポーネント型名。
   static constexpr const char* kTypeName = "GamepadGuideTextComponent";
   /// @brief エディターに表示する日本語名と英語名。
   static constexpr GameEngine::ComponentDisplayName kDisplayName{"ゲームパッド操作ガイド", "Gamepad Guide Text"};

   const char* GetTypeName() const override { return kTypeName; }

   /// @brief 表示条件に使用するRaceManagerを解決する
   void OnReferencesChanged(GameEngine::SceneWorld& sceneWorld) override;

   /// @brief リザルト中だけ操作ガイドを非表示にする
   void Update(float deltaTime) override;

   nlohmann::json Serialize() const override;
   void Deserialize(const nlohmann::json& data) override;

#ifdef USE_IMGUI
   void DrawInspector() override;
#endif

private:
   std::string raceManagerId_;                   ///< レース進行を参照する管理Objectの安定Entity ID。
   std::string playerObjectId_;                  ///< 操作対象プレイヤーの安定Entity ID。
   RaceManagerComponent* raceManager_ = nullptr; ///< 参照解決済みレース管理コンポーネントへの非所有参照。
   CharacterJump* characterJump_ = nullptr;      ///< プレイヤーのジャンプ入力と空中状態を扱うコンポーネントへの参照。
};

} // namespace App
