#pragma once

#include "GameEngine/Object/Component/Base/IObjectComponent.h"

namespace GameEngine {
class Camera;
}

namespace App {

class RaceManagerComponent;

/// @brief 車体の上方にゴールへ向くワールド空間の矢印を配置する。
class RaceGoalDirectionWorldComponent final : public GameEngine::IObjectComponent {
public:
   static constexpr const char* kTypeName = "RaceGoalDirectionWorldComponent"; ///< 保存用の型名。
   static constexpr const char* kObjectId = "Model:GoalDirectionHUD";          ///< HUDと共用する矢印モデルの安定ID。
   static constexpr GameEngine::ComponentDisplayName kDisplayName{
      "ゴール方向（車体上）", "Goal Direction Above Vehicle"};

   /// @brief 保存とファクトリ登録に使うコンポーネント名を返す。
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief 矢印の追従先、ゴール、レースの参照を現在のワールドから解決する。
   /// @param sceneWorld 所有オブジェクトが属するワールド。
   void OnReferencesChanged(GameEngine::SceneWorld& sceneWorld) override;

   /// @brief 無効になったときに矢印を非表示にする。
   void OnDisable() override;

   /// @brief シミュレーション更新後の姿勢から車体上の矢印を配置する。
   /// @param camera 描画に使うカメラ。未設定の場合は矢印を非表示にする。
   void Present(const GameEngine::Camera* camera);

   /// @brief チュートリアルの進行に合わせて矢印の表示可否を更新する。
   /// @param enabled 誘導を許可する場合true。
   void SetGuidanceEnabled(bool enabled);

   /// @brief 参照IDと車体上の配置設定を保存用JSONへ変換する。
   nlohmann::json Serialize() const override;

   /// @brief 参照ID、車体からの高さ（m）、モデルの拡大率を検証して読み込む。
   /// @param data コンポーネント固有の設定。
   void Deserialize(const nlohmann::json& data) override;

private:
   bool ShouldShow() const;
   void Hide();

   std::string playerId_;
   std::string goalId_;
   std::string raceManagerId_; ///< 空の場合はレース管理による表示制御を使わない。
   float heightAboveVehicle_ = 0.f; ///< 車体原点からカメラの上方向に測る矢印中心までの距離（m）。
   float modelScale_ = 0.f;         ///< 矢印モデルの一様な拡大率。
   const GameEngine::Object* player_ = nullptr;
   const GameEngine::Object* goal_ = nullptr;
   const RaceManagerComponent* raceManager_ = nullptr;
   bool guidanceEnabled_ = true;
};

} // namespace App
