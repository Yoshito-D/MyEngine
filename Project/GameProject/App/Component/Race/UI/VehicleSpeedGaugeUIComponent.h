#pragma once

#include "GameEngine/Object/Component/Base/IObjectComponent.h"
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
   static constexpr const char* kTypeName = "VehicleSpeedGaugeUIComponent"; ///< コンポーネント型名。
   /// @brief エディターに表示する日本語名と英語名。
   static constexpr GameEngine::ComponentDisplayName kDisplayName{"車両速度ゲージ", "Vehicle Speed Gauge"};

   /// @brief コンポーネント型名を取得する
   /// @return VehicleSpeedGaugeUIComponent
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief プレイヤー、レース、枠、速度テキストへの参照を解決する
   /// @param sceneWorld 所属するシーンワールド
   void OnSceneLoaded(GameEngine::SceneWorld& sceneWorld) override;

   /// @copydoc GameEngine::IObjectComponent::OnReferencesChanged
   void OnReferencesChanged(GameEngine::SceneWorld& sceneWorld) override;

   /// @brief 速度比率に応じてゲージの幅とUV範囲を更新する
   /// @param deltaTime ゲーム用デルタタイム（秒）
   void Update(float deltaTime) override;

   /// @brief 参照先、ゲージ寸法、応答速度、単位変換係数をJSONへ保存する
   /// @return 保存用JSON
   nlohmann::json Serialize() const override;

   /// @brief JSONから参照先とゲージ表示設定を読み込む
   /// @param data ゲージ設定JSON
   void Deserialize(const nlohmann::json& data) override;

   /// @copydoc GameEngine::ParameterComponent::GetSharedParameters
   nlohmann::json GetSharedParameters() const override;

   /// @copydoc GameEngine::ParameterComponent::ApplySharedParameters
   void ApplySharedParameters(const nlohmann::json& data) override;

#ifdef USE_IMGUI

   /// @brief ゲージ設定と参照解決状態をインスペクターへ表示する
   void DrawInspector() override;
#endif

private:
   void ReadParameters(const nlohmann::json& data, bool initializeRuntime);

   void SetHudVisible(bool visible);

   std::string raceManagerId_ = "Object:RaceManager";        ///< レース進行を参照する管理Objectの安定Entity ID。
   std::string playerObjectId_ = "Model:Player";             ///< 操作対象プレイヤーの安定Entity ID。
   std::string frameObjectId_ = "Sprite:SpeedGaugeFrame";    ///< 速度ゲージの枠を表示するObjectの安定Entity ID。
   std::string speedTextObjectId_ = "UIText:SpeedGaugeText"; ///< 速度数値を表示するUITextの安定Entity ID。

   float gaugeWidth_ = 360.0f;  ///< 速度ゲージの基準横幅（ピクセル）。
   float gaugeHeight_ = 121.0f; ///< 速度ゲージの基準高さ（ピクセル）。
   float response_ = 8.0f;      ///< 実速度へ表示値を近づける補間応答速度。
   float unitToKmh_ = 3.6f;     ///< ワールド速度を表示用km/hへ変換する倍率。

   RaceManagerComponent* raceManager_ = nullptr; ///< 参照解決済みレース管理コンポーネントへの非所有参照。
   GravityBody* gravityBody_ = nullptr;          ///< プレイヤーの速度・重力・姿勢を参照するコンポーネント。
   VehicleGroundMover* groundMover_ = nullptr;   ///< 地上走行速度を参照するコンポーネントへの非所有参照。
   GameEngine::Sprite* gaugeSprite_ = nullptr;   ///< 速度に応じて表示範囲を変えるゲージスプライトへの参照。
   /// @brief 速度ゲージの表示状態を切り替える描画コンポーネントへの参照。
   GameEngine::RenderComponent* gaugeRender_ = nullptr;
   GameEngine::Object* frameObject_ = nullptr;        ///< 速度ゲージの枠を表示するObjectへの非所有参照。
   GameEngine::UITextComponent* speedText_ = nullptr; ///< 速度数値の文章を更新するUITextコンポーネントへの参照。
   float textureWidth_ = 1.0f;                        ///< ゲージ画像の元の横幅（ピクセル）。
   float textureHeight_ = 1.0f;                       ///< ゲージ画像の元の高さ（ピクセル）。
   float displayedRatio_ = 0.0f;                      ///< 補間後にゲージへ適用する現在の速度割合。
};

} // namespace App
