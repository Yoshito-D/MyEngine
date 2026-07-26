#pragma once

#include "Object/Component/IObjectComponent.h"
#include <string>

namespace GameEngine {
class Object;
}

namespace App {

class RaceManagerComponent;

/// @brief プレイヤーからゴールまでの直線距離をメートル表記でUITextへ表示する
class RaceGoalDistanceTextComponent final : public GameEngine::IObjectComponent {
public:
   static constexpr const char* kTypeName = "RaceGoalDistanceTextComponent";
   static constexpr GameEngine::ComponentDisplayName kDisplayName{ "ゴール距離表示", "Goal Distance Text" };

   /// @brief コンポーネント型名を取得する
   /// @return RaceGoalDistanceTextComponent
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief レース、プレイヤー、ゴールへの参照を解決する
   /// @param sceneWorld 所属するシーンワールド
   void OnSceneLoaded(GameEngine::SceneWorld& sceneWorld) override;

   /// @brief レース中のゴール距離を整数メートルでUITextへ反映する
   /// @param deltaTime 未使用
   void Update(float deltaTime) override;

   /// @brief 参照IDをJSONへ保存する
   /// @return 保存用JSON
   nlohmann::json Serialize() const override;

   /// @brief JSONから参照IDを読み込む
   /// @param data 表示設定JSON
   void Deserialize(const nlohmann::json& data) override;

#ifdef USE_IMGUI
   /// @brief 参照IDと解決状態をインスペクターへ表示する
   void DrawInspector() override;
#endif

private:
   std::string raceManagerId_ = "Object:RaceManager";
   std::string playerObjectId_ = "Model:Player";
   std::string goalObjectId_ = "Model:Goal";
   RaceManagerComponent* raceManager_ = nullptr;
   GameEngine::Object* playerObject_ = nullptr;
   GameEngine::Object* goalObject_ = nullptr;
};

} // namespace App
