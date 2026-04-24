#include "TestScene.h"
#include "Framework/EngineContext.h"
#include "../Component/Gravity/GravityBody.h"
#include "../Component/Gravity/SphericalGravityAttractor.h"
#include "../Component/Gravity/GravityAttractorLink.h"
#include "../Component/Gravity/PlanetSwitcher.h"
#include "../Component/Player/PlayerController.h"
#include "../Component/Camera/ScreenSpaceBasis.h"
#include "../Component/Character/CharacterWalker.h"
#include "../Component/Character/CharacterJump.h"
#include "../Component/Character/CharacterLanding.h"
#include "../Component/Camera/CameraGravityBridge.h"
#include "../Component/Camera/GravityFollowCamera.h"
#include "../Component/Camera/PlanetLeashCamera.h"
#include "Component/TransformComponent.h"
#include "Component/AnimationComponent.h"
#include "Component/RenderComponent.h"
#include "MathUtils.h"
#include <cmath>

using namespace GameEngine;
using namespace App;

static constexpr float kPlanetRadius = 15.0f;
static constexpr float kPlayerOrbitHeight = kPlanetRadius;

void TestScene::Initialize() {
   BaseScene::Initialize();

   // --- アセット読み込み ---
   EngineContext::LoadModel("resources/models/planet", "planet.obj");
   EngineContext::LoadModel("resources/models/human", "walk.gltf");
   EngineContext::LoadAnimation("resources/models/human", "walk.gltf");
   EngineContext::LoadTexture("resources/textures/uvChecker.png", "uvChecker");

   EngineContext::CreateMaterial("planetMaterial", 0xffffffff, 3);
   EngineContext::CreateMaterial("playerMaterial", 0xffffffff, 3);

   auto planetMaterial = EngineContext::GetMaterial("planetMaterial");
   auto playerMaterial = EngineContext::GetMaterial("playerMaterial");
   auto planetModelAsset = EngineContext::GetModel("planet.obj");
   auto playerModelAsset = EngineContext::GetModel("walk.gltf");

   // --- 惑星1の作成 ---
   planet_ = std::make_unique<Model>();
   planet_->Create().SetModelAsset(planetModelAsset).SetMaterial(planetMaterial);
   planet_->SetPosition(Vector3(0.0f, 0.0f, 0.0f));
   planet_->SetScale(Vector3(kPlanetRadius, kPlanetRadius, kPlanetRadius));

   if (auto* render = planet_->GetComponent<RenderComponent>()) {
	  render->textureName = "uvChecker";
   }

   SphericalGravityAttractor* attractor1 = nullptr;
   if (auto* a = planet_->AddComponent<SphericalGravityAttractor>()) {
	  a->influenceRadius = kPlanetRadius * 2.5f;
	  attractor1 = a;
   }

   // --- 惑星2の作成 ---
   planet2_ = std::make_unique<Model>();
   planet2_->Create().SetModelAsset(planetModelAsset).SetMaterial(planetMaterial);
   planet2_->SetPosition(Vector3(kPlanet2Distance, 0.0f, 0.0f));
   planet2_->SetScale(Vector3(kPlanet2Radius, kPlanet2Radius, kPlanet2Radius));

   if (auto* render = planet2_->GetComponent<RenderComponent>()) {
	  render->textureName = "uvChecker";
   }

   SphericalGravityAttractor* attractor2 = nullptr;
   if (auto* a = planet2_->AddComponent<SphericalGravityAttractor>()) {
	  a->influenceRadius = kPlanet2Radius * 2.5f;
	  attractor2 = a;
   }

   // --- プレイヤーの作成 ---
   player_ = std::make_unique<Model>();
   player_->Create().SetModelAsset(playerModelAsset).SetMaterial(playerMaterial);
   player_->SetPosition(Vector3(0.0f, kPlayerOrbitHeight, 0.0f));
   player_->SetScale(Vector3(1.0f, 1.0f, 1.0f));

   // GravityBody: 姿勢制御 + 物理
   if (auto* gravityBody = player_->AddComponent<GravityBody>()) {
	  gravityBody->rotationSpeed = 5.0f;
	  gravityBody->gravityStrength = 9.8f;
	  gravityBody->useGravity = true;
   }

   // GravityAttractorLink: 初期惑星（惑星1）の重力を適用（PlanetSwitcher が切り替える）
   if (auto* link = player_->AddComponent<GravityAttractorLink>()) {
	  link->SetAttractor(attractor1);
   }

   // CharacterLanding: 初期惑星（惑星1）の表面に固定
   if (auto* landing = player_->AddComponent<CharacterLanding>()) {
	  landing->SetPlanetCenter(planet_->GetPosition());
	  landing->surfaceRadius_ = kPlanetRadius;
   }

   // PlanetSwitcher: 最近傍の惑星に自動乗り換え
   if (auto* switcher = player_->AddComponent<PlanetSwitcher>()) {
	  switcher->AddPlanet(attractor1, planet_->GetPosition(), kPlanetRadius);
	  switcher->AddPlanet(attractor2, planet2_->GetPosition(), kPlanet2Radius);
   }

   // ScreenSpaceBasis: カメラ軸の重力平面投影（カメラは後で設定）
   player_->AddComponent<ScreenSpaceBasis>();

   // CharacterWalker: 水平移動 + yaw補間
   if (auto* walker = player_->AddComponent<CharacterWalker>()) {
	  walker->moveSpeed = 4.0f;
   }

   // CharacterJump: ジャンプ初速
   player_->AddComponent<CharacterJump>();

   // PlayerController: 入力収集
   player_->AddComponent<PlayerController>();

   // AnimationComponent
   if (auto* anim = player_->AddComponent<AnimationComponent>()) {
	  anim->animationName = "walk.gltf";
	  anim->playing = true;
	  anim->loop = true;
   }

   // --- 仮想カメラのセットアップ ---
   mainVcam_ = std::make_unique<VirtualCamera>();
   mainVcam_->Initialize();
   mainVcam_->SetPriority(0);
   gravityFollowCamera_ = mainVcam_->AddComponent<GravityFollowCamera>();
   if (gravityFollowCamera_) {
	  gravityFollowCamera_->SetDistance(15.0f);
   }
   if (auto* brain = EngineContext::GetActiveBrain()) {
	  brain->RegisterVirtualCamera(mainVcam_.get());
   }

   leashVcam_ = std::make_unique<VirtualCamera>();
   leashVcam_->Initialize();
   leashVcam_->SetPriority(-1);
   leashCamera_ = leashVcam_->AddComponent<PlanetLeashCamera>();
   if (leashCamera_) {
	  leashCamera_->maxFollowDistance = 30.0f;
	  leashCamera_->followSpeed = 15.0f;
	  leashCamera_->minPlanetDistance = kPlanetRadius + 20.0f;
	  leashCamera_->useGravityUp = true;
	  leashCamera_->SetInitialEyePosition(Vector3(0.0f, kPlayerOrbitHeight + 5.0f, -15.0f));
   }

   if (auto* brain = EngineContext::GetActiveBrain()) {
	  brain->RegisterVirtualCamera(leashVcam_.get());
   }

   // CameraGravityBridge: gravityUp / pivotTarget をカメラへ毎フレーム通知（PlanetSwitcher が中心座標を切り替える）
   if (auto* bridge = player_->AddComponent<CameraGravityBridge>()) {
	  bridge->SetPlanetCenter(planet_->GetPosition());
	  bridge->SetGravityFollowCamera(gravityFollowCamera_);
	  bridge->SetPlanetLeashCamera(leashCamera_);
   }

   // PlayerController にカメラを設定（ScreenSpaceBasis へ転送される）
   if (auto* controller = player_->GetComponent<PlayerController>()) {
	  controller->SetGravityFollowCamera(gravityFollowCamera_);
	  controller->SetPlanetLeashCamera(nullptr); // 初期は GravityFollow を使用
   }

#ifdef USE_IMGUI
   isDebugCameraActive_ = false;
#endif
}

void TestScene::Update() {
   BaseScene::Update();

   float deltaTime = EngineContext::GetDeltaTime();
   testTime_ += deltaTime;

   // Tab キーで GravityFollowCamera / PlanetLeashCamera を切り替え
   if (EngineContext::IsKeyTriggered(KeyCode::Tab)) {
	  useLeashCamera_ = !useLeashCamera_;
	  if (mainVcam_)  mainVcam_->SetPriority(useLeashCamera_ ? -1 : 0);
	  if (leashVcam_) leashVcam_->SetPriority(useLeashCamera_ ? 0 : -1);

	  if (auto* controller = player_->GetComponent<PlayerController>()) {
		 controller->SetGravityFollowCamera(useLeashCamera_ ? nullptr : gravityFollowCamera_);
		 controller->SetPlanetLeashCamera(useLeashCamera_ ? leashCamera_ : nullptr);
	  }
	  if (auto* bridge = player_->GetComponent<CameraGravityBridge>()) {
		 bridge->SetGravityFollowCamera(useLeashCamera_ ? nullptr : gravityFollowCamera_);
		 bridge->SetPlanetLeashCamera(useLeashCamera_ ? leashCamera_ : nullptr);
	  }
   }

   // デバッグ描画
   if (planet_ && player_) {
	  Vector3 planetPos = planet_->GetPosition();
	  Vector3 playerPos = player_->GetPosition();

	  EngineContext::DrawLine(planetPos, playerPos, Vector4(1.0f, 1.0f, 1.0f, 0.5f));

	  if (auto* gravityBody = player_->GetComponent<GravityBody>()) {
		 Vector3 currentUp = gravityBody->GetCurrentUpVector() * 2.0f;
		 EngineContext::DrawLine(playerPos, playerPos + currentUp, Vector4(0.0f, 1.0f, 0.0f, 1.0f));
	  }
	  if (auto* controller = player_->GetComponent<PlayerController>()) {
		 Vector3 fProj = controller->GetLastForwardProjected() * 2.0f;
		 Vector3 rProj = controller->GetLastRightProjected() * 2.0f;
		 Vector3 moveDir = controller->GetLastMoveDirection() * 2.5f;
		 EngineContext::DrawLine(playerPos, playerPos + fProj, Vector4(0.0f, 0.0f, 1.0f, 1.0f));
		 EngineContext::DrawLine(playerPos, playerPos + rProj, Vector4(1.0f, 0.0f, 0.0f, 1.0f));
		 if (moveDir.LengthSquared() > 1e-4f) {
			EngineContext::DrawLine(playerPos, playerPos + moveDir, Vector4(1.0f, 1.0f, 0.0f, 1.0f));
		 }
	  }

	  if (auto* attractor = planet_->GetComponent<SphericalGravityAttractor>()) {
		 float r = attractor->influenceRadius;
		 EngineContext::DrawLine(planetPos + Vector3(r, 0, 0), planetPos - Vector3(r, 0, 0), Vector4(1, 1, 0, 0.3f));
		 EngineContext::DrawLine(planetPos + Vector3(0, r, 0), planetPos - Vector3(0, r, 0), Vector4(1, 1, 0, 0.3f));
		 EngineContext::DrawLine(planetPos + Vector3(0, 0, r), planetPos - Vector3(0, 0, r), Vector4(1, 1, 0, 0.3f));
	  }
   }

   // 惑星2のデバッグ描画
   if (planet2_) {
	  Vector3 p2Pos = planet2_->GetPosition();
	  EngineContext::DrawLine(p2Pos, player_->GetPosition(), Vector4(0.5f, 0.5f, 1.0f, 0.3f));
	  if (auto* attractor = planet2_->GetComponent<SphericalGravityAttractor>()) {
		 float r = attractor->influenceRadius;
		 EngineContext::DrawLine(p2Pos + Vector3(r, 0, 0), p2Pos - Vector3(r, 0, 0), Vector4(0, 1, 1, 0.3f));
		 EngineContext::DrawLine(p2Pos + Vector3(0, r, 0), p2Pos - Vector3(0, r, 0), Vector4(0, 1, 1, 0.3f));
		 EngineContext::DrawLine(p2Pos + Vector3(0, 0, r), p2Pos - Vector3(0, 0, r), Vector4(0, 1, 1, 0.3f));
	  }
   }
}

void TestScene::Draw() {
   BaseScene::Draw();
}
