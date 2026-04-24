#pragma once

#include "GravityAttractor.h"
#include "Object/Model/ModelAsset.h"
#include <memory>

namespace App {

/// @brief メッシュ法線ベースの重力発生源コンポーネント
class MeshNormalGravityAttractor : public GravityAttractor {
public:
   static constexpr const char* kTypeName = "MeshNormalGravityAttractor";
   const char* GetTypeName() const override { return kTypeName; }

   void Update(float deltaTime) override { (void)deltaTime; }

   bool IsInRange(const GameEngine::Vector3& /*objectPosition*/) const override { return true; }

   GameEngine::Vector3 GetUpVectorFor(const GameEngine::Vector3& objectPosition) const override {
      GameEngine::Vector3 normal = FindSurfaceNormal(objectPosition);
      if (normal.LengthSquared() > 1e-8f) { return normal.Normalize(); }
      return fallbackUpVector_;
   }

   void SetFallbackUpVector(const GameEngine::Vector3& up) {
      if (up.LengthSquared() > 1e-8f) { fallbackUpVector_ = up.Normalize(); }
   }

   nlohmann::json Serialize() const override {
      nlohmann::json json;
      json["fallbackUp"] = { fallbackUpVector_.x, fallbackUpVector_.y, fallbackUpVector_.z };
      return json;
   }

   void Deserialize(const nlohmann::json& data) override {
      if (data.contains("fallbackUp")) {
         auto up = data["fallbackUp"];
         fallbackUpVector_ = { up[0], up[1], up[2] };
      }
   }

#ifdef USE_IMGUI
   void DrawInspector() override;
#endif

protected:
   virtual GameEngine::Vector3 FindSurfaceNormal(const GameEngine::Vector3& /*objectPosition*/) const {
      return { 0.0f, 0.0f, 0.0f };
   }

protected:
   GameEngine::Vector3 fallbackUpVector_ = { 0.0f, 1.0f, 0.0f };
};

} // namespace App
