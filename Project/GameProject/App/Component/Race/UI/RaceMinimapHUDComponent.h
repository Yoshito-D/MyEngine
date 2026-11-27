#pragma once

#include "GameEngine/Graphics/Renderer/Pass/SphereMapRenderer.h"
#include "GameEngine/Math/Types/Quaternion.h"
#include "GameEngine/Object/Component/Base/IObjectComponent.h"
#include <optional>

namespace App {

class PlanetSwitcher;
class RaceGoalDirectionHUDComponent;
class RaceManagerComponent;
class VehicleInputComponent;

/// @brief プレイヤー周辺の回転可能な円形ミニマップとゴール直線距離を管理する。
class RaceMinimapHUDComponent final : public GameEngine::IObjectComponent {
public:
   static constexpr const char* kTypeName = "RaceMinimapHUDComponent"; ///< 保存用の型名。
   static constexpr const char* kObjectId = "Object:RaceMinimapHUD";    ///< 各コースに置くHUDの安定ID。
   static constexpr GameEngine::ComponentDisplayName kDisplayName{"レースミニマップ", "Race Minimap"};

   /// @brief ミニマップの投影と外観を初期化する。
   RaceMinimapHUDComponent();

   /// @brief コンポーネントの保存用型名を返す。
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief シーンの開始姿勢を保存し、プレイヤーの前方を地図の上側に合わせる。
   /// @param sceneWorld HUDを所有するシーン。
   void OnSceneLoaded(GameEngine::SceneWorld& sceneWorld) override;

   /// @brief プレイヤー、入力部品、惑星の切り替え部品、ゴールへの参照を解決する。
   /// @param sceneWorld HUDを所有するシーン。
   void OnReferencesChanged(GameEngine::SceneWorld& sceneWorld) override;

   /// @brief ミニマップが表示されている間、視点入力から回転を更新する。
   /// @param deltaTime ゲームのシミュレーション経過時間（秒）。
   void Update(float deltaTime) override;

   /// @brief ゲームの更新完了後に、そのフレームのミニマップを提出する。
   /// @param goalDirection 距離表示に並べる矢印。シーンに矢印がない場合はnullptr。
   void Submit(RaceGoalDirectionHUDComponent* goalDirection);

   /// @brief 地図の表示中に対象プレイヤーの視点入力を受け持つかを返す。
   /// @param player 視点入力を使うプレイヤー。
   bool HandlesViewInputFor(const GameEngine::Object& player) const;

   /// @brief チュートリアルの進行に応じて地図と距離の表示を許可する。
   /// @param enabled 表示を許可する場合true。
   void SetGuidanceEnabled(bool enabled) { guidanceEnabled_ = enabled; }

   /// @brief プレイヤー等の参照ID、フォントID、初期視点の見下ろし角（rad）を保存する。
   nlohmann::json Serialize() const override;

   /// @brief シーン固有の参照と見下ろし角（rad）を検証して読み込む。
   /// @param data ミニマップの設定JSON。
   void Deserialize(const nlohmann::json& data) override;

#ifdef USE_IMGUI
   /// @brief ミニマップの参照先、距離表示フォントと初期角度を編集する。
   void DrawInspector() override;
#endif

private:
   bool ShouldShow() const;
   GameEngine::Vector3 ToView(const GameEngine::Vector3& worldPosition) const;
   GameEngine::Vector3 ToViewDirection(const GameEngine::Vector3& worldDirection) const;
   void AddGoalPoint(const GameEngine::Vector3& viewPosition);

   std::string playerId_;
   std::string goalId_;
   std::string raceManagerId_;
   float initialPitch_ = 0.f;  ///< 初期視点の見下ろし角（rad）。
   float yaw_ = 0.f;           ///< プレイヤーの開始方向を基準とした水平角（rad）。
   float pitch_ = 0.f;         ///< 現在の視点の見下ろし角（rad）。
   GameEngine::Vector2 yawCosSin_{};
   GameEngine::Vector2 pitchCosSin_{};
   GameEngine::Vector2 angularVelocity_{}; ///< 左右・上下の回転速度（rad/s）。
   std::optional<GameEngine::Quaternion> worldToStart_; ///< ワールド方向をプレイヤーの開始姿勢へ戻す回転。
   GameEngine::Object* player_ = nullptr;
   GameEngine::Object* goal_ = nullptr;
   RaceManagerComponent* raceManager_ = nullptr;
   PlanetSwitcher* planetSwitcher_ = nullptr;
   const VehicleInputComponent* vehicleInput_ = nullptr;
   bool guidanceEnabled_ = true;
   bool fontValidated_ = false;
   GameEngine::Vector3 viewCenter_{}; ///< 軌道の中心となるプレイヤーのワールド座標（m）。
   GameEngine::SphereMapFrame frame_{};
};

} // namespace App
