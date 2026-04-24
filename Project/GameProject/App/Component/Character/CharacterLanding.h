#pragma once

#include "Object/Component/IObjectComponent.h"
#include "Utility/Math/Vector3.h"

namespace App {

/// @brief 惑星表面への着地判定・表面固定・着地時慣性リセットを担うコンポーネント
///
/// - ジャンプ中: 落下して surfaceRadius_ 以下に到達したら着地処理
/// - 地上: 表面に固定し GravityBody / CharacterWalker の速度をゼロにする
class CharacterLanding final : public GameEngine::IObjectComponent {
public:
   static constexpr const char* kTypeName = "CharacterLanding";
   const char* GetTypeName() const override { return kTypeName; }

   void Update(float deltaTime) override;

   void SetPlanetCenter(const GameEngine::Vector3& center) { planetCenter_ = center; }
   void SetSurfaceRadius(float radius) { surfaceRadius_ = radius; }

   bool IsGrounded() const { return isGrounded_; }

#ifdef USE_IMGUI
   void DrawInspector() override;
#endif

   nlohmann::json Serialize() const override;
   void Deserialize(const nlohmann::json& data) override;

public:
   float surfaceRadius_ = 15.0f;

private:
   GameEngine::Vector3 planetCenter_ = { 0.0f, 0.0f, 0.0f };
   bool                isGrounded_   = true;
};

} // namespace App
