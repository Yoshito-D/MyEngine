#pragma once

#include "Object/Component/IObjectComponent.h"
#include "Utility/Math/Vector3.h"

namespace App {

/// @brief 着地直後の速度ブーストを担うコンポーネント（オプション）
///
/// - VehicleMover が着地を検出したとき TryBoost() を呼び出す
/// - 車体上方と gravityUp の並行度が threshold 以上のとき
///   VehicleGroundMover の速度に boostAmount を加算する
/// - このコンポーネントを Object から外すだけでブーストを無効化できる
class VehicleLandingBoost final : public GameEngine::IObjectComponent {
public:
   static constexpr const char* kTypeName = "VehicleLandingBoost";
   const char* GetTypeName() const override { return kTypeName; }
   void Update(float deltaTime) override { (void)deltaTime; }

   /// @brief 着地時にブーストを試みる
   /// @param localUp   着地瞬間の車体上方ベクトル
   /// @param gravityUp 現在の重力Up方向
   /// @return ブーストを発動したなら true
   bool TryBoost(const GameEngine::Vector3& localUp,
				 const GameEngine::Vector3& gravityUp);

#ifdef USE_IMGUI
   void DrawInspector() override;
#endif

   nlohmann::json Serialize() const override;
   void Deserialize(const nlohmann::json& data) override;

public:
   /// @brief 着地ブーストの加算量（units/sec）
   float boostAmount    = 10.0f;

   /// @brief ブーストが発動する並行度の閾値（dot 積。1 が完全平行）
   float boostThreshold = 0.9f;
};

} // namespace App
