#pragma once
#include "BaseScene.h"
#include "Model.h"
#include "Camera/Core/VirtualCamera.h"
#include "../Component/Camera/GravityFollowCamera.h"
#include "../Component/Camera/PlayerRearFollowCamera.h"
#include "../Component/Camera/PlanetLeashCamera.h"
#include "Object/Skybox/Skybox.h"
#include "Object/Component/ParticleEmitterComponent.h"
#include <memory>

/// @brief ゲーム機能テスト用シーン（重力システム等）
class GameTestScene : public GameEngine::BaseScene {
public:
   void Initialize() override;
   void Update() override;
   void Draw() override;

private:
   enum class CameraMode {
	  PlayerRearFollow,
	  GravityFollow,
	  PlanetLeash,
   };

   // 惑星1
   std::unique_ptr<GameEngine::Model> planet_ = nullptr;
   // 惑星2
   std::unique_ptr<GameEngine::Model> planet2_ = nullptr;

   std::unique_ptr<GameEngine::Model> player_ = nullptr;

   std::unique_ptr<GameEngine::VirtualCamera> mainVcam_ = nullptr;
   App::GravityFollowCamera* gravityFollowCamera_ = nullptr;

   std::unique_ptr<GameEngine::VirtualCamera> rearFollowVcam_ = nullptr;
   App::PlayerRearFollowCamera* playerRearFollowCamera_ = nullptr;

   std::unique_ptr<GameEngine::VirtualCamera> leashVcam_ = nullptr;
   App::PlanetLeashCamera* leashCamera_ = nullptr;

   CameraMode cameraMode_ = CameraMode::PlayerRearFollow;
   float testTime_ = 0.0f;

   std::unique_ptr<GameEngine::Skybox> skybox_ = nullptr;

   // タイヤ埃パーティクル
   GameEngine::ParticleEmitterComponent* tireDustEmitter_ = nullptr;
   bool wasDrifting_ = false;
   int tireDustSlotCount_ = 0;

   // ソニックブームパーティクル（ミニターボ発動時の一発再生）
   GameEngine::ParticleEmitterComponent* sonicBoomEmitter_ = nullptr;
   int sonicBoomSlotIndex_ = -1;

   GameEngine::ParticleEmitterComponent* landingRingEmitter_ = nullptr;
   int landingRingSlotIndex_ = -1;

   GameEngine::ParticleEmitterComponent* landingEmitter_ = nullptr;
   int landingSlotIndex_ = -1;

   // 惑星2 の配置パラメータ
   static constexpr float kPlanet2Radius = 20.0f;
   static constexpr float kPlanet2Distance = 53.0f; ///< 惑星1中心からの距離
};
