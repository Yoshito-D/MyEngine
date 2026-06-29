#pragma once

#include "GravityAttractor.h"

namespace App {

/// @brief 中心から放射状に重力Upを返す球状重力発生源
class SphericalGravityAttractor final : public GravityAttractor {
public:
   /// @brief コンポーネント種別名
   static constexpr const char* kTypeName = "SphericalGravityAttractor";
   static constexpr GameEngine::ComponentDisplayName kDisplayName{ "球状重力アトラクター", "Spherical Gravity Attractor" };

   /// @brief 型名を返す
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief 影響半径内かどうかを返す（0以下は無限範囲）
   bool IsInRange(const GameEngine::Vector3& objectPosition) const override;

   /// @brief 中心から対象への方向を重力Upとして返す
   GameEngine::Vector3 GetUpVectorFor(const GameEngine::Vector3& objectPosition) const override;

   /// @brief influenceRadius をシリアライズする
   nlohmann::json Serialize() const override {
      nlohmann::json json;
      json["influenceRadius"] = influenceRadius;
      return json;
   }

   /// @brief influenceRadius をデシリアライズする
   void Deserialize(const nlohmann::json& data) override {
      if (data.contains("influenceRadius")) { influenceRadius = data["influenceRadius"]; }
   }

#ifdef USE_IMGUI
   /// @brief デバッグ表示（Inspector）
   void DrawInspector() override;
#endif

public:
   /// @brief 影響半径（0以下で無限）
   float influenceRadius = 0.0f;

private:
   /// @brief 発生源中心座標（オーナーTransform）
   GameEngine::Vector3 GetCenter() const {
      if (!HasOwner()) { return { 0.0f, 0.0f, 0.0f }; }
      auto* t = GetOwner().GetComponent<GameEngine::TransformComponent>();
      return t ? t->transform.translation : GameEngine::Vector3{ 0.0f, 0.0f, 0.0f };
   }
};

} // namespace App
