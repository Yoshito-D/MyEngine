#pragma once

#include "Object/Component/IObjectComponent.h"
#include "GravityAttractor.h"

namespace App {

/// @brief 指定した GravityAttractor を同オーナーの GravityBody へ橋渡しするコンポーネント
class GravityAttractorLink final : public GameEngine::IObjectComponent {
public:
   /// @brief コンポーネント種別名
   static constexpr const char* kTypeName = "GravityAttractorLink";

   /// @brief 型名を返す
   const char* GetTypeName() const override { return kTypeName; }

   /// @brief 現在の attractor を GravityBody へ適用する
   void Update(float deltaTime) override;

   /// @brief 適用元となる重力発生源を設定する
   void SetAttractor(GravityAttractor* attractor) { attractor_ = attractor; }

#ifdef USE_IMGUI
   /// @brief デバッグ表示（Inspector）
   void DrawInspector() override;
#endif

   /// @brief シリアライズ（参照のみのため保存項目なし）
   nlohmann::json Serialize() const override  { return {}; }

   /// @brief デシリアライズ（保存項目なしのため処理なし）
   void Deserialize(const nlohmann::json&) override {}

private:
   /// @brief 現在接続中の重力発生源
   GravityAttractor* attractor_ = nullptr;
};

} // namespace App
