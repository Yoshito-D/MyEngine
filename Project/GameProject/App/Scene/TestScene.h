#pragma once
#include "BaseScene.h"
#include "Model.h"
#include "Camera/Core/VirtualCamera.h"
#include "../Component/Camera/GravityFollowCamera.h"
#include "../Component/Camera/PlanetLeashCamera.h"
#include <memory>

/// @brief 重力システムテスト用シーン
class TestScene : public GameEngine::BaseScene {
public:
   void Initialize() override;
   void Update() override;
   void Draw() override;

private:
   // 惑星1
   std::unique_ptr<GameEngine::Model> planet_  = nullptr;
   // 惑星2
   std::unique_ptr<GameEngine::Model> planet2_ = nullptr;

   std::unique_ptr<GameEngine::Model> player_ = nullptr;

   std::unique_ptr<GameEngine::VirtualCamera> mainVcam_ = nullptr;
   App::GravityFollowCamera* gravityFollowCamera_ = nullptr;

   std::unique_ptr<GameEngine::VirtualCamera> leashVcam_ = nullptr;
   App::PlanetLeashCamera* leashCamera_ = nullptr;

   bool  useLeashCamera_ = false;
   float testTime_       = 0.0f;

   // 惑星2 の配置パラメータ
   static constexpr float kPlanet2Radius   = 10.0f;
   static constexpr float kPlanet2Distance = 30.0f; ///< 惑星1中心からの距離
};
