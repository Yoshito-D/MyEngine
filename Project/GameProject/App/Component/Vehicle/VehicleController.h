#pragma once

#include "Object/Component/IObjectComponent.h"
#include "Utility/Math/Vector3.h"
#include "../Gravity/GravityBody.h"
#include "../Character/CharacterJump.h"
#include "../Camera/GravityFollowCamera.h"
#include "VehicleInputComponent.h"
#include "VehicleMover.h"

namespace App {

/// @brief 入力を収集して車の移動・ジャンプへ振り分けるコンポーネント
class VehicleController final : public GameEngine::IObjectComponent {
public:
   /// @brief コンポーネント種別名
   static constexpr const char* kTypeName = "VehicleController";
   static constexpr GameEngine::ComponentDisplayName kDisplayName{ "車両制御", "Vehicle Controller" };

   /// @brief 型名を返す
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief 入力取得と各サブコンポーネントへの委譲を行う
   void Update(float deltaTime) override;

   /// @brief GravityFollowCamera 参照を設定する
   void SetGravityFollowCamera(GravityFollowCamera* cam);

   /// @brief 直近の移動方向を取得する
   GameEngine::Vector3 GetLastMoveDirection() const;

#ifdef USE_IMGUI
   /// @brief デバッグ表示（Inspector）
   void DrawInspector() override;
#endif

   /// @brief パラメータをシリアライズする
   nlohmann::json Serialize() const override;

   /// @brief パラメータをデシリアライズする
   void Deserialize(const nlohmann::json& data) override;

private:
   /// @brief 選択中カメラの安定ID。削除されたComponentの生ポインターを保持しない
   std::string gravityFollowCameraId_;
};

} // namespace App
