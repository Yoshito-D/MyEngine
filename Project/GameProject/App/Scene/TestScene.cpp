#include "TestScene.h"
#include "Framework/EngineContext.h"
#include "../Component/GravityBody.h"
#include "../Component/SphericalGravityAttractor.h"
#include "../Component/PlayerController.h"
#include "../Component/GravityFollowCamera.h"
#include "Component/TransformComponent.h"
#include "Component/AnimationComponent.h"
#include "MathUtils.h"
#include <cmath>

using namespace GameEngine;

// 惑星の半径（モデルスケール換算）
static constexpr float kPlanetRadius = 5.0f;
// プレイヤーの初期位置（惑星表面）
static constexpr float kPlayerOrbitHeight = kPlanetRadius + 1.5f;

void TestScene::Initialize() {
   BaseScene::Initialize();

   // --- アセット読み込み ---
   EngineContext::LoadModel("resources/models/planet", "planet.obj");
   EngineContext::LoadModel("resources/models/human", "walk.gltf");
   EngineContext::LoadAnimation("resources/models/human", "walk.gltf");

   EngineContext::CreateMaterial("planetMaterial", 0xffffffff, 3);
   EngineContext::CreateMaterial("playerMaterial", 0xffffffff, 3);

   auto planetMaterial = EngineContext::GetMaterial("planetMaterial");
   auto playerMaterial = EngineContext::GetMaterial("playerMaterial");
   auto planetModelAsset = EngineContext::GetModel("planet.obj");
   auto playerModelAsset = EngineContext::GetModel("walk.gltf");

   // --- 惑星の作成（SphericalGravityAttractorをアタッチ） ---
   planet_ = std::make_unique<Model>();
   planet_->Create().SetModelAsset(planetModelAsset).SetMaterial(planetMaterial);
   planet_->SetPosition(Vector3(0.0f, 0.0f, 0.0f));
   planet_->SetScale(Vector3(kPlanetRadius, kPlanetRadius, kPlanetRadius));

   // TransformComponentを追加
   planet_->AddComponent<TransformComponent>();

   // SphericalGravityAttractorをアタッチ
   if (auto* attractor = planet_->AddComponent<SphericalGravityAttractor>()) {
	  // 影響半径は惑星スケールの2倍（広めに設定）
	  attractor->influenceRadius = kPlanetRadius * 2.0f;
   }

   // --- プレイヤーの作成（GravityBodyをアタッチ） ---
   player_ = std::make_unique<Model>();
   player_->Create().SetModelAsset(playerModelAsset).SetMaterial(playerMaterial);
   // 惑星表面（+Y方向）に配置
   player_->SetPosition(Vector3(0.0f, kPlayerOrbitHeight, 0.0f));
   player_->SetScale(Vector3(1.0f, 1.0f, 1.0f));

   player_->AddComponent<TransformComponent>();

   if (auto* gravityBody = player_->AddComponent<GravityBody>()) {
	  gravityBody->rotationSpeed = 5.0f;
	  gravityBody->gravityStrength = 9.8f;
	  gravityBody->useGravity = false;  // フェーズ3では姿勢制御のみ（物理落下なし）
   }

   // --- フェーズ4: 重力追従型OrbitalCamera付き仮想カメラを作成 ---
   mainVcam_ = std::make_unique<GameEngine::VirtualCamera>();
   mainVcam_->Initialize();
   mainVcam_->SetPriority(0);
   gravityFollowCamera_ = mainVcam_->AddComponent<GameEngine::GravityFollowCamera>();
   if (gravityFollowCamera_) {
	  gravityFollowCamera_->SetDistance(15.0f);
   }

   // CinemachineBrainに登録
   if (auto* brain = EngineContext::GetActiveBrain()) {
	  brain->RegisterVirtualCamera(mainVcam_.get());
   }

   // PlayerControllerをアタッチ
   if (auto* controller = player_->AddComponent<PlayerController>()) {
	  controller->moveSpeed = 4.0f;
	  // フェーズ4: GravityFollowCameraをセット（スクリーンスペース投影用）
	  controller->SetGravityFollowCamera(gravityFollowCamera_);
   }

   // ウォークアニメーションを設定
   if (auto* anim = player_->AddComponent<AnimationComponent>()) {
	  anim->animationName = "walk.gltf";
	  anim->playing = true;
	  anim->loop = true;
	  anim->applyRotation = false;
	  anim->applyScale = false;
	  anim->applyTranslation = false;
   }

#ifdef USE_IMGUI
   isDebugCameraActive_ = false;
#endif
}

void TestScene::Update() {
   BaseScene::Update();

   float deltaTime = EngineContext::GetDeltaTime();
   testTime_ += deltaTime;

   // SphericalGravityAttractorがプレイヤーのGravityBodyに重力方向を適用
   if (planet_ && player_) {
	  auto* attractor  = planet_->GetComponent<SphericalGravityAttractor>();
	  auto* gravityBody = player_->GetComponent<GravityBody>();

	  if (attractor && gravityBody) {
		 Vector3 playerPos = player_->GetPosition();
		 attractor->ApplyTo(*gravityBody, playerPos);

		 // プレイヤーを惑星表面（半径 + オフセット）に固定する
		 // コリジョンがないため、GravityBodyのUpVector方向に距離をキープ
		 Vector3 planetPos  = planet_->GetPosition();
		 Vector3 toPlayer   = playerPos - planetPos;
		 float   dist       = toPlayer.Length();
		 if (dist > 1e-4f) {
			Vector3 surfacePos = planetPos + toPlayer.Normalize() * kPlayerOrbitHeight;
			player_->SetPosition(surfacePos);
		 }
	  }

	  // フェーズ4: GravityFollowCameraにgravityUpとpivotTargetを毎フレーム更新
	  Vector3 playerPos = player_->GetPosition();
	  if (gravityFollowCamera_) {
		 Vector3 gravityUp = (playerPos - planet_->GetPosition());
		 float len = gravityUp.Length();
		 if (len > 1e-4f) gravityFollowCamera_->SetGravityUp(gravityUp * (1.0f / len));
		 gravityFollowCamera_->SetPivotTarget(playerPos);
	  }
   }

   // デバッグ描画
   if (planet_ && player_) {
	  Vector3 planetPos = planet_->GetPosition();
	  Vector3 playerPos = player_->GetPosition();

	  // 惑星→プレイヤーの重力方向（白）
	  EngineContext::DrawLine(planetPos, playerPos, Vector4(1.0f, 1.0f, 1.0f, 0.5f));

	  // プレイヤーのUpVector（緑）・F_proj（青）・R_proj（赤）
	  if (auto* gravityBody = player_->GetComponent<GravityBody>()) {
		 Vector3 currentUp = gravityBody->GetCurrentUpVector() * 2.0f;
		 EngineContext::DrawLine(playerPos, playerPos + currentUp, Vector4(0.0f, 1.0f, 0.0f, 1.0f));
	  }
	  if (auto* controller = player_->GetComponent<PlayerController>()) {
		 Vector3 fProj = controller->GetLastForwardProjected() * 2.0f;
		 Vector3 rProj = controller->GetLastRightProjected()   * 2.0f;
		 Vector3 moveDir = controller->GetLastMoveDirection()  * 2.5f;
		 EngineContext::DrawLine(playerPos, playerPos + fProj,   Vector4(0.0f, 0.0f, 1.0f, 1.0f));
		 EngineContext::DrawLine(playerPos, playerPos + rProj,   Vector4(1.0f, 0.0f, 0.0f, 1.0f));
		 if (moveDir.LengthSquared() > 1e-4f) {
			EngineContext::DrawLine(playerPos, playerPos + moveDir, Vector4(1.0f, 1.0f, 0.0f, 1.0f));
		 }
	  }

	  // 惑星の影響範囲（黄）
	  if (auto* attractor = planet_->GetComponent<SphericalGravityAttractor>()) {
		 float r = attractor->influenceRadius;
		 EngineContext::DrawLine(planetPos + Vector3(r, 0, 0), planetPos - Vector3(r, 0, 0), Vector4(1.0f, 1.0f, 0.0f, 0.3f));
		 EngineContext::DrawLine(planetPos + Vector3(0, r, 0), planetPos - Vector3(0, r, 0), Vector4(1.0f, 1.0f, 0.0f, 0.3f));
		 EngineContext::DrawLine(planetPos + Vector3(0, 0, r), planetPos - Vector3(0, 0, r), Vector4(1.0f, 1.0f, 0.0f, 0.3f));
	  }
   }
}

void TestScene::Draw() {
   BaseScene::Draw();
}
