#pragma once

#include "GravityAttractor.h"
#include "Object/Model/ModelAsset.h"
#include <memory>

namespace App {

/// @brief メッシュ法線を重力Upとして利用する重力発生源
class MeshNormalGravityAttractor : public GravityAttractor {
public:
   /// @brief コンポーネント種別名
   static constexpr const char* kTypeName = "MeshNormalGravityAttractor";

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

   /// @brief 法線取得失敗時の代替Upを設定する
   void SetFallbackUpVector(const GameEngine::Vector3& up) {
      if (up.LengthSquared() > 1e-8f) { fallbackUpVector_ = up.Normalize(); }
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
         auto up = data["fallbackUp"];
         fallbackUpVector_ = { up[0], up[1], up[2] };
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

protected:
   /// @brief 法線取得不能時の代替Up
   GameEngine::Vector3 fallbackUpVector_ = { 0.0f, 1.0f, 0.0f };
};

} // namespace App
