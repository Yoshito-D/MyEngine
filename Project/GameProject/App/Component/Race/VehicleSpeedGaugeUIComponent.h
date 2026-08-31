#pragma once

#include "Object/Component/IObjectComponent.h"
#include <string>

namespace GameEngine {
class Object;
class RenderComponent;
class Sprite;
class UITextComponent;
}

namespace App {

class RaceManagerComponent;
class GravityBody;
class VehicleGroundMover;

/// @brief 車両速度を三角形ゲージと数値テキストへ反映する
class VehicleSpeedGaugeUIComponent final : public GameEngine::IObjectComponent {
public:
   static constexpr const char* kTypeName = "VehicleSpeedGaugeUIComponent";
   static constexpr GameEngine::ComponentDisplayName kDisplayName{ "車両速度ゲージ", "Vehicle Speed Gauge" };

   /// @brief コンポーネント型名を取得する
   /// @return VehicleSpeedGaugeUIComponent
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief プレイヤー、レース、枠、速度テキストへの参照を解決する
   /// @param sceneWorld 所属するシーンワールド
   void OnSceneLoaded(GameEngine::SceneWorld& sceneWorld) override;

   /// @brief 速度比率に応じてゲージの幅とUV範囲を更新する
   /// @param deltaTime ゲーム用デルタタイム（秒）
   void Update(float deltaTime) override;

   /// @brief 参照先、ゲージ寸法、応答速度、単位変換係数をJSONへ保存する
   /// @return 保存用JSON
   nlohmann::json Serialize() const override;

   /// @brief JSONから参照先とゲージ表示設定を読み込む
   /// @param data ゲージ設定JSON
   void Deserialize(const nlohmann::json& data) override;

#ifdef USE_IMGUI
   /// @brief ゲージ設定と参照解決状態をインスペクターへ表示する
   void DrawInspector() override;
#endif

private:
   void SetHudVisible(bool visible);

   std::string raceManagerId_ = "Object:RaceManager";
   std::string playerObjectId_ = "Model:Player";
   std::string frameObjectId_ = "Sprite:SpeedGaugeFrame";
   std::string speedTextObjectId_ = "UIText:SpeedGaugeText";

   float gaugeWidth_ = 360.0f;
   float gaugeHeight_ = 121.0f;
   float response_ = 8.0f;
   float unitToKmh_ = 3.6f;

   RaceManagerComponent* raceManager_ = nullptr;
   GravityBody* gravityBody_ = nullptr;
   VehicleGroundMover* groundMover_ = nullptr;
   GameEngine::Sprite* gaugeSprite_ = nullptr;
   GameEngine::RenderComponent* gaugeRender_ = nullptr;
   GameEngine::Object* frameObject_ = nullptr;
   GameEngine::UITextComponent* speedText_ = nullptr;
   float textureWidth_ = 1.0f;
   float textureHeight_ = 1.0f;
   float displayedRatio_ = 0.0f;
};

} // namespace App
