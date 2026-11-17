#pragma once

#include "GameEngine/Object/Component/Base/IObjectComponent.h"
#include <string>

namespace App {

class CharacterJump;
class RaceManagerComponent;
class VehicleInputComponent;

/// @brief 接続状態に応じたゲームパッドまたはキーボードの操作ガイドをリザルト以外でUITextへ表示する。
class GamepadGuideTextComponent final : public GameEngine::IObjectComponent {
public:
   /// @brief シーン保存とファクトリ検索で使うコンポーネント型名。
   static constexpr const char* kTypeName = "GamepadGuideTextComponent";
   /// @brief エディターに表示する日本語名と英語名。
   static constexpr GameEngine::ComponentDisplayName kDisplayName{"操作ガイド", "Input Guide Text"};

   const char* GetTypeName() const override { return kTypeName; }

   /// @brief 表示条件に使用するレース管理・ジャンプ状態・プレイヤーの入力設定を解決する。
   void OnReferencesChanged(GameEngine::SceneWorld& sceneWorld) override;

   /// @brief 接続状態と地上・空中・導入の状態に合わせて操作ガイドを切り替え、リザルト中は非表示にする。
   void Update(float deltaTime) override;

   nlohmann::json Serialize() const override;
   void Deserialize(const nlohmann::json& data) override;

#ifdef USE_IMGUI
   void DrawInspector() override;
#endif

private:
   std::string raceManagerId_;                     ///< レース進行を参照する管理Objectの安定Entity ID。
   std::string playerObjectId_;                    ///< 操作対象プレイヤーの安定Entity ID。
   RaceManagerComponent* raceManager_ = nullptr;   ///< 参照解決済みレース管理コンポーネントへの非所有参照。
   CharacterJump* characterJump_ = nullptr;        ///< プレイヤーのジャンプ入力と空中状態を扱うコンポーネントへの参照。
   VehicleInputComponent* vehicleInput_ = nullptr; ///< 接続確認に使うプレイヤースロットを保持する非所有参照。
};

} // namespace App
