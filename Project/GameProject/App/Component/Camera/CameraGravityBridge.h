#pragma once

#include "Object/Component/IObjectComponent.h"
#include "Utility/Math/Vector3.h"
#include "GravityFollowCamera.h"
#include "PlanetLeashCamera.h"

namespace App {

/// @brief 自身の位置・重力Up方向を GravityFollowCamera / PlanetLeashCamera へ毎フレーム通知するコンポーネント
class CameraGravityBridge final : public GameEngine::IObjectComponent {
public:
   static constexpr const char* kTypeName = "CameraGravityBridge";
   const char* GetTypeName() const override { return kTypeName; }

   void Update(float deltaTime) override;

   void SetPlanetCenter(const GameEngine::Vector3& center)    { planetCenter_        = center; }
   void SetGravityFollowCamera(GravityFollowCamera* cam)      { gravityFollowCamera_ = cam; }
   void SetPlanetLeashCamera(PlanetLeashCamera* cam)          { planetLeashCamera_   = cam; }

#ifdef USE_IMGUI
   void DrawInspector() override;
#endif

   nlohmann::json Serialize() const override  { return {}; }
   void Deserialize(const nlohmann::json&) override {}

private:
   GameEngine::Vector3  planetCenter_        = { 0.0f, 0.0f, 0.0f };
   GravityFollowCamera* gravityFollowCamera_ = nullptr;
   PlanetLeashCamera*   planetLeashCamera_   = nullptr;
};

} // namespace App
