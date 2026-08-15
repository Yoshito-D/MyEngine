#pragma once

#include "Object/Component/IObjectComponent.h"
#include <string>

namespace GameEngine {
class Object;
class Model;
class UIModelComponent;
}

namespace App {

class RaceManagerComponent;

/// @brief プレイヤーからゴールへの方向を3D矢印で示す
class RaceGoalDirectionHUDComponent final : public GameEngine::IObjectComponent {
public:
   static constexpr const char* kTypeName = "RaceGoalDirectionHUDComponent";
   static constexpr GameEngine::ComponentDisplayName kDisplayName{ "ゴール方向3D矢印", "Goal Direction 3D Arrow" };

   /// @brief コンポーネント型名を取得する
   /// @return RaceGoalDirectionHUDComponent
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief レース、プレイヤー、ゴールへの参照を解決する
   /// @param sceneWorld 所属するシーンワールド
   void OnSceneLoaded(GameEngine::SceneWorld& sceneWorld) override;

   /// @brief プレイヤーとゴールの位置から3D矢印の向きを更新する
   /// @param deltaTime ゲーム用デルタタイム（秒）
   void Update(float deltaTime) override;

   /// @brief 3D矢印の参照設定をJSONへ保存する
   /// @return 保存用JSON
   nlohmann::json Serialize() const override;

   /// @brief JSONから3D矢印の参照設定を読み込む
   /// @param data 3D矢印設定JSON
   void Deserialize(const nlohmann::json& data) override;

#ifdef USE_IMGUI
   /// @brief HUD設定と参照状態をインスペクターへ表示する
   void DrawInspector() override;
#endif

private:
   void SetVisible(bool visible);

   std::string raceManagerId_ = "Object:RaceManager";
   std::string playerObjectId_ = "Model:Player";
   std::string goalObjectId_ = "Model:Goal";
   RaceManagerComponent* raceManager_ = nullptr;
   GameEngine::Object* playerObject_ = nullptr;
   GameEngine::Object* goalObject_ = nullptr;
   GameEngine::Model* arrowModel_ = nullptr;
   GameEngine::UIModelComponent* uiModelComponent_ = nullptr;
};

} // namespace App
