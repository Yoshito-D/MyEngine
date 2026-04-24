#pragma once

#include "GravityAttractor.h"

namespace App {

/// @brief 球状重力の発生源コンポーネント
class SphericalGravityAttractor final : public GravityAttractor {
public:
   static constexpr const char* kTypeName = "SphericalGravityAttractor";
   const char* GetTypeName() const override { return kTypeName; }

   void Update(float deltaTime) override { (void)deltaTime; }

   bool IsInRange(const GameEngine::Vector3& objectPosition) const override {
      if (influenceRadius <= 0.0f) { return true; }
      GameEngine::Vector3 diff = objectPosition - GetCenter();
      return diff.LengthSquared() <= influenceRadius * influenceRadius;
   }

   GameEngine::Vector3 GetUpVectorFor(const GameEngine::Vector3& objectPosition) const override {
      GameEngine::Vector3 dir = objectPosition - GetCenter();
      if (dir.LengthSquared() < 1e-8f) { return GameEngine::Vector3{ 0.0f, 1.0f, 0.0f }; }
      return dir.Normalize();
   }

   nlohmann::json Serialize() const override {
      nlohmann::json json;
      json["influenceRadius"] = influenceRadius;
      return json;
   }

   void Deserialize(const nlohmann::json& data) override {
      if (data.contains("influenceRadius")) { influenceRadius = data["influenceRadius"]; }
   }

#ifdef USE_IMGUI
   void DrawInspector() override;
#endif

public:
   float influenceRadius = 0.0f;

private:
   GameEngine::Vector3 GetCenter() const {
      if (!HasOwner()) { return { 0.0f, 0.0f, 0.0f }; }
      auto* t = GetOwner().GetComponent<GameEngine::TransformComponent>();
      return t ? t->transform.translation : GameEngine::Vector3{ 0.0f, 0.0f, 0.0f };
   }
};

} // namespace App
