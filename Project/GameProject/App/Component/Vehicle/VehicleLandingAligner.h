#pragma once

#include "Object/Component/IObjectComponent.h"
#include "Utility/Math/Vector3.h"
#include "Utility/Math/Quaternion.h"

namespace App {

/// @brief 着地直後に Slerp で姿勢を gravityUp に合わせるコンポーネント
///
/// - VehicleMover が着地を検出したとき BeginAlign() を呼び出す
/// - タイマーが残っている間は VehicleGroundMover の姿勢再構築をスキップさせる
/// - Update() を毎フレーム呼ぶことで補正を進行させる
class VehicleLandingAligner final : public GameEngine::IObjectComponent {
public:
   static constexpr const char* kTypeName = "VehicleLandingAligner";
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief 毎フレーム Slerp 補正を進行させる
   void Update(float deltaTime) override;

   /// @brief 着地補正を開始する
   /// @param startRot  着地した瞬間の回転
   /// @param targetRot gravityUp に合わせた目標回転
   void BeginAlign(const GameEngine::Quaternion& startRot,
				   const GameEngine::Quaternion& targetRot);

   /// @brief 補正中かどうかを返す
   bool IsAligning() const { return alignTimer_ > 0.0f; }

#ifdef USE_IMGUI
   void DrawInspector() override;
#endif

   nlohmann::json Serialize() const override;
   void Deserialize(const nlohmann::json& data) override;

public:
   /// @brief 姿勢補正にかける時間（秒）
   float alignTime = 0.1f;

private:
   /// @brief 現在のタイマーから Slerp 済みクォータニオンを計算する
   GameEngine::Quaternion CalcBlendedRotation() const;

   /// @brief blended を Transform と GravityBody に適用する
   void ApplyBlendedRotation(const GameEngine::Quaternion& blended);

   float alignTimer_ = 0.0f;
   GameEngine::Quaternion alignStart_  = { 0.0f, 0.0f, 0.0f, 1.0f };
   GameEngine::Quaternion alignTarget_ = { 0.0f, 0.0f, 0.0f, 1.0f };
};

} // namespace App
