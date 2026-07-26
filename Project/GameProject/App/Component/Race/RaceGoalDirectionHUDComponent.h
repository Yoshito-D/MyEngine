#pragma once

#include "Object/Component/IObjectComponent.h"
#include "Utility/Math/Vector2.h"
#include <string>

namespace GameEngine {
class Object;
class Sprite;
}

namespace App {

class GravityBody;
class RaceManagerComponent;

/// @brief ゴール追従表示と右下固定の方位表示を切り替えられるHUD
class RaceGoalDirectionHUDComponent final : public GameEngine::IObjectComponent {
public:
   static constexpr const char* kTypeName = "RaceGoalDirectionHUDComponent";
   static constexpr GameEngine::ComponentDisplayName kDisplayName{ "ゴール方向HUD", "Goal Direction HUD" };

   /// @brief コンポーネント型名を取得する
   /// @return RaceGoalDirectionHUDComponent
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief レース、プレイヤー、ゴールへの参照を解決する
   /// @param sceneWorld 所属するシーンワールド
   void OnSceneLoaded(GameEngine::SceneWorld& sceneWorld) override;

   /// @brief ゴール位置からHUDの表示位置と向きを更新する
   /// @param deltaTime ゲーム用デルタタイム（秒）
   void Update(float deltaTime) override;

   /// @brief HUD参照と追従設定をJSONへ保存する
   /// @return 保存用JSON
   nlohmann::json Serialize() const override;

   /// @brief JSONからHUD参照と追従設定を読み込む
   /// @param data HUD設定JSON
   void Deserialize(const nlohmann::json& data) override;

#ifdef USE_IMGUI
   /// @brief HUD設定と参照状態をインスペクターへ表示する
   void DrawInspector() override;
#endif

private:
   struct HudTarget {
      GameEngine::Vector2 position{};
      float rotation = 0.0f;
   };

   bool TryCalculateTarget(HudTarget& target);
   void SetVisible(bool visible);
   void ResetFollowState();

   std::string raceManagerId_ = "Object:RaceManager";
   std::string playerObjectId_ = "Model:Player";
   std::string goalObjectId_ = "Model:Goal";
   float edgePadding_ = 24.0f;
   float positionFollowSpeed_ = 2400.0f;
   float rotationFollowSpeed_ = 10.0f;
   bool fixedBottomRight_ = false;
   RaceManagerComponent* raceManager_ = nullptr;
   GameEngine::Object* playerObject_ = nullptr;
   GameEngine::Object* goalObject_ = nullptr;
   GravityBody* gravityBody_ = nullptr;
   GameEngine::Sprite* sprite_ = nullptr;
   GameEngine::Vector2 currentPosition_{};
   float currentRotation_ = 0.0f;
   bool followInitialized_ = false;
   bool useProjectedPosition_ = false;
   bool lastFixedBottomRight_ = false;
};

} // namespace App
