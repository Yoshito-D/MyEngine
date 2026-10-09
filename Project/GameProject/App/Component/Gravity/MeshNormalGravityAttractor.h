#pragma once

#include "GameProject/App/Component/Gravity/GravityAttractor.h"
#include "GameEngine/Assets/Model/ModelAsset.h"
#include <memory>
#include <cmath>

namespace App {

/// @brief メッシュ法線を重力Upとして利用する重力発生源
class MeshNormalGravityAttractor : public GravityAttractor {
public:
   /// @brief コンポーネント種別名
   static constexpr const char* kTypeName = "MeshNormalGravityAttractor";
   static constexpr GameEngine::ComponentDisplayName kDisplayName{ "メッシュ法線重力アトラクター", "Mesh Normal Gravity Attractor" };

   /// @brief 型名を返す
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief 毎フレーム更新（本体は問い合わせ駆動）
   void Update(float deltaTime) override { (void)deltaTime; }

   /// @brief 影響範囲判定（現状は常に有効）
   bool IsInRange(const GameEngine::Vector3& /*objectPosition*/) const override { return true; }

   /// @brief 指定位置の法線を重力Upとして返す（失敗時はfallback）
   GameEngine::Vector3 GetUpVectorFor(const GameEngine::Vector3& objectPosition) const override {
      GameEngine::Vector3 normal = FindSurfaceNormal(objectPosition);
      if (normal.LengthSquared() > 1e-8f) { return normal.Normalize(); }
      return fallbackUpVector_;
   }

   /// @brief 有限かつ非ゼロの代替Upを正規化して設定する。不正値では現在値を保持する。
   void SetFallbackUpVector(const GameEngine::Vector3& up) {
      const float length = std::hypot(up.x, up.y, up.z);
      if (!std::isfinite(length) || length <= 1.0e-4f) return;
      fallbackUpVector_ = up / length;
   }

   /// @brief fallbackUp をシリアライズする
   nlohmann::json Serialize() const override {
      nlohmann::json json;
      json["fallbackUp"] = { fallbackUpVector_.x, fallbackUpVector_.y, fallbackUpVector_.z };
      return json;
   }

   /// @brief fallbackUp をデシリアライズする
   void Deserialize(const nlohmann::json& data) override {
      if (data.contains("fallbackUp")) {
         const auto& up = data.at("fallbackUp");
         if (up.is_array() && up.size() == 3 &&
            up[0].is_number() && up[1].is_number() && up[2].is_number()) {
            SetFallbackUpVector({ up[0].get<float>(), up[1].get<float>(), up[2].get<float>() });
         }
      }
   }

#ifdef USE_IMGUI
   /// @brief デバッグ表示（Inspector）
   void DrawInspector() override;
#endif

protected:
   /// @brief 指定位置の表面法線を返す（派生で実装）
   virtual GameEngine::Vector3 FindSurfaceNormal(const GameEngine::Vector3& /*objectPosition*/) const {
      return { 0.0f, 0.0f, 0.0f };
   }

private:
   /// @brief 法線取得不能時の代替Up
   GameEngine::Vector3 fallbackUpVector_ = { 0.0f, 1.0f, 0.0f };
};

} // namespace App
