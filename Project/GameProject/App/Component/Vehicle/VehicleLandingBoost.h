#pragma once

#include "Object/Component/IObjectComponent.h"
#include "Utility/Math/Vector3.h"

namespace App {

/// @brief 着地判定の結果
enum class LandingResult {
   Success, ///< 成功: ブースト加算
   Normal,  ///< 普通: 速度変化なし
   Failure, ///< 失敗: 減速
};

/// @brief 着地直後の速度ブーストを担うコンポーネント（オプション）
///
/// - VehicleMover が着地を検出したとき TryBoost() を呼び出す
/// - 着地角度（alignment）に応じて3段階の判定を行う
///   - alignment >= boostThreshold  → 成功: boostAmount を加算
///   - alignment >= normalThreshold → 普通: 速度変化なし
///   - alignment <  normalThreshold → 失敗: penaltyAmount を減算
/// - このコンポーネントを Object から外すだけでブーストを無効化できる
class VehicleLandingBoost final : public GameEngine::IObjectComponent {
public:
   static constexpr const char* kTypeName = "VehicleLandingBoost";
   const char* GetTypeName() const override { return kTypeName; }
   void Update(float deltaTime) override { (void)deltaTime; }

   /// @brief 着地時に判定を行い速度を調整する
   /// @param localUp   着地瞬間の車体上方ベクトル
   /// @param gravityUp 現在の重力Up方向
   /// @return 着地判定結果
   LandingResult TryBoost(const GameEngine::Vector3& localUp,
						  const GameEngine::Vector3& gravityUp);

#ifdef USE_IMGUI
   void DrawInspector() override;
#endif

   nlohmann::json Serialize() const override;
   void Deserialize(const nlohmann::json& data) override;

public:
   /// @brief 着地ブーストの加算量（units/sec）
   float boostAmount     = 0.0f;

   /// @brief ブーストが発動する平行度の閾値（dot 積。1 が完全平行）
   float boostThreshold  = 0.95f;

   /// @brief 普通と失敗の境界となる平行度の閾値
   float normalThreshold = 0.65f;

   /// @brief 失敗着地の減速量（units/sec）
   float penaltyAmount   = 0.0f;
};

} // namespace App
