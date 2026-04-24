#pragma once

#include "Object/Component/IObjectComponent.h"
#include "GravityAttractor.h"

namespace App {

/// @brief 指定した GravityAttractor を毎フレーム同オーナーの GravityBody へ自動適用するコンポーネント
class GravityAttractorLink final : public GameEngine::IObjectComponent {
public:
   static constexpr const char* kTypeName = "GravityAttractorLink";
   const char* GetTypeName() const override { return kTypeName; }

   void Update(float deltaTime) override;

   void SetAttractor(GravityAttractor* attractor) { attractor_ = attractor; }

#ifdef USE_IMGUI
   void DrawInspector() override;
#endif

   nlohmann::json Serialize() const override  { return {}; }
   void Deserialize(const nlohmann::json&) override {}

private:
   GravityAttractor* attractor_ = nullptr;
};

} // namespace App
