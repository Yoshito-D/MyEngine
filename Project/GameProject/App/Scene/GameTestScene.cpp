#include "GameTestScene.h"
#include "Framework/EngineContext.h"
#include "../Component/Gravity/GravityBody.h"
#include "../Component/Gravity/SphericalGravityAttractor.h"
#include "../Component/Gravity/GravityAttractorLink.h"
#include "../Component/Gravity/PlanetSwitcher.h"
#include "../Component/Vehicle/VehicleController.h"
#include "../Component/Vehicle/VehicleMover.h"
#include "../Component/Vehicle/VehicleGroundMover.h"
#include "../Component/Vehicle/VehicleAirController.h"
#include "../Component/Vehicle/VehicleLandingAligner.h"
#include "../Component/Vehicle/VehicleLandingBoost.h"
#include "../Component/Vehicle/VehicleDrift.h"
#include "../Component/Character/CharacterJump.h"
#include "../Component/Character/CharacterLanding.h"
#include "../Component/Camera/CameraGravityBridge.h"
#include "../Component/Camera/GravityFollowCamera.h"
#include "../Component/Camera/PlayerRearFollowCamera.h"
#include "../Component/Camera/PlanetLeashCamera.h"
#include "Component/TransformComponent.h"
#include "Component/AnimationComponent.h"
#include "Component/RenderComponent.h"
#include "Component/MaterialComponent.h"
#include "Effect/ParticleSystem.h"
#include "MathUtils.h"
#include <cmath>

#ifdef USE_IMGUI
#include "ImguiManager.h"
#endif

using namespace GameEngine;
using namespace App;

static constexpr float kPlanetRadius = 25.0f;
static constexpr float kPlayerOrbitHeight = kPlanetRadius;

void GameTestScene::Initialize() {
   BaseScene::Initialize();

   EngineContext::LoadTexture("resources/textures/space_2048px.dds", "skyboxTexture");
   EngineContext::LoadTexture("resources/textures/gradationLine.png", "gradationLine");
   EngineContext::LoadTexture("resources/textures/smoke.png", "smoke");


   skybox_ = std::make_unique<GameEngine::Skybox>();
   skybox_->Create(EngineContext::GetGraphicsDevice());
   skybox_->SetTexture(EngineContext::GetTexture("skyboxTexture"));

   // --- アセット読み込み ---
   EngineContext::LoadModel("resources/models/planet", "planet.obj");
   EngineContext::LoadModel("resources/models/car", "car.obj");
   EngineContext::LoadTexture("resources/textures/uvChecker.png", "uvChecker");
   EngineContext::LoadTexture("resources/textures/color_palette.png", "car");

   EngineContext::CreateMaterial("planetMaterial", 0xffffffff, 0);
   EngineContext::CreateMaterial("playerMaterial", 0xffffffff, 3);

   auto planetMaterial = EngineContext::GetMaterial("planetMaterial");
   auto playerMaterial = EngineContext::GetMaterial("playerMaterial");
   auto planetModelAsset = EngineContext::GetModel("planet.obj");
   auto playerModelAsset = EngineContext::GetModel("car.obj");

   playerMaterial->SetEnvironmentTextureStrength(0.7f);

   // --- 惑星1の作成 ---
   planet_ = std::make_unique<Model>();
   planet_->Create().SetModelAsset(planetModelAsset).SetMaterial(planetMaterial);
   planet_->SetPosition(Vector3(0.0f, 0.0f, 0.0f));
   planet_->SetScale(Vector3(kPlanetRadius, kPlanetRadius, kPlanetRadius));

   SphericalGravityAttractor* attractor1 = nullptr;
   if (auto* a = planet_->AddComponent<SphericalGravityAttractor>()) {
	  a->influenceRadius = kPlanetRadius * 10.0f;
	  attractor1 = a;
   }
   // uvChecker テクスチャを設定
   if (auto* mc = planet_->GetComponent<MaterialComponent>()) { mc->SetTextureName("uvChecker"); }

   // --- 惑星2の作成 ---
   planet2_ = std::make_unique<Model>();
   planet2_->Create().SetModelAsset(planetModelAsset).SetMaterial(planetMaterial);
   planet2_->SetPosition(Vector3(kPlanet2Distance, 0.0f, 0.0f));
   planet2_->SetScale(Vector3(kPlanet2Radius, kPlanet2Radius, kPlanet2Radius));

   SphericalGravityAttractor* attractor2 = nullptr;
   if (auto* a = planet2_->AddComponent<SphericalGravityAttractor>()) {
	  a->influenceRadius = kPlanet2Radius * 10.0f;
	  attractor2 = a;
   }
   // uvChecker テクスチャを設定
   if (auto* mc = planet2_->GetComponent<MaterialComponent>()) { mc->SetTextureName("uvChecker"); }

   // --- プレイヤーの作成 ---
   player_ = std::make_unique<Model>();
   player_->Create().SetModelAsset(playerModelAsset).SetMaterial(playerMaterial);
   player_->SetPosition(Vector3(0.0f, kPlayerOrbitHeight, 0.0f));
   player_->SetScale(Vector3(1.0f, 1.0f, 1.0f));

   if (auto* mc = player_->GetComponent<MaterialComponent>()) { mc->SetTextureName("car"); }

   // 1. GravityAttractorLink: 今フレームの重力方向を先に確定させる
   if (auto* link = player_->AddComponent<GravityAttractorLink>()) {
	  link->SetAttractor(attractor1);
   }

   // 2. PlanetSwitcher: GravityAttractorLink の接続先を切り替える
   if (auto* switcher = player_->AddComponent<PlanetSwitcher>()) {
	  switcher->AddPlanet(attractor1, planet_->GetPosition(), kPlanetRadius);
	  switcher->AddPlanet(attractor2, planet2_->GetPosition(), kPlanet2Radius);
   }

   // 3. GravityBody: 確定済みの重力方向で姿勢回転 + 位置移動
   if (auto* gravityBody = player_->AddComponent<GravityBody>()) {
	  gravityBody->rotationSpeed = 10.0f;
	  gravityBody->gravityStrength = 22.0f;
	  gravityBody->useGravity = true;
   }

   // 4. CharacterLanding: 位置を地表にスナップ・垂直速度除去
   if (auto* landing = player_->AddComponent<CharacterLanding>()) {
	  landing->SetPlanetCenter(planet_->GetPosition());
	  landing->surfaceRadius_ = kPlanetRadius;
   }

   // 5. CharacterJump: ジャンプ初速
   player_->AddComponent<CharacterJump>();

   // 6. VehicleMover: coordinator（VehicleController から呼ばれる）
   player_->AddComponent<VehicleMover>();

   // 6a. VehicleGroundMover: 地上前進・ステアリング・姿勢再構築
   player_->AddComponent<VehicleGroundMover>();

   // 6b. VehicleAirController: 空中 yaw/pitch 回転（慣性付き）
   player_->AddComponent<VehicleAirController>();

   // 6c. VehicleLandingAligner: 着地後の Slerp 姿勢補正（外せば無効化）
   player_->AddComponent<VehicleLandingAligner>();

   // 6d. VehicleLandingBoost: 着地時の速度ブースト（外せば無効化）
   player_->AddComponent<VehicleLandingBoost>();

   // 6e. VehicleDrift: ドリフト処理（外せば無効化）
   //      デフォルトは SustainedSteer モード（追加ボタン不要）。
   //      miniTurboEnabled = false でミニターボなしのシンプルなドリフトになる。
   if (auto* drift = player_->AddComponent<VehicleDrift>()) {
	  drift->miniTurboEnabled = true;
   }

   // 7. VehicleController: 入力収集 → VehicleMover 呼び出し（最後に姿勢を確定）
   player_->AddComponent<VehicleController>();

   // 8. タイヤ埃パーティクル（ドリフト時にのみ表示）
   tireDustEmitter_ = player_->AddComponent<GameEngine::ParticleEmitterComponent>();
   if (tireDustEmitter_) {
	  using Config = GameEngine::ParticleEmitterComponent::AttachmentConfig;
	  tireDustSlotCount_ = 0;

	  // 各タイヤのローカルオフセット（右前・左前・右後・左後）
	  const GameEngine::Vector3 kTireOffsets[4] = {
		 {  0.3f, -0.2f,  0.415f },   // 右前
		 { -0.3f, -0.2f,  0.415f },   // 左前
		 {  0.3f, -0.2f, -0.415f },   // 右後
		 { -0.3f, -0.2f, -0.415f },   // 左後
	  };

	  for (const auto& offset : kTireOffsets) {
		 Config cfg;
		 cfg.followPosition = true;
		 cfg.followRotation = true;
		 cfg.followScale = true;
		 cfg.positionOffset = offset;
		 cfg.simulationSpace = Config::Space::World;
		 int slotIdx = tireDustEmitter_->AddSlot("resources/particles/tire_dust.json", cfg);
		 if (auto* slot = tireDustEmitter_->GetSlot(slotIdx)) {
			slot->loop = true;
			// AddSlot 後は既に Play() 済みのため、EmissionModule だけ無効化して放出を止める。
			// isPlaying_ は true のままにすることで、後からEmissionを有効にするだけで再開できる。
			if (slot->particleSystem) {
			   if (auto* em = slot->particleSystem->GetEmissionModule()) {
				  em->SetEnabled(false);
			   }
			}
			++tireDustSlotCount_;
		 }
	  }
   }

   // 9. ソニックブームパーティクル（ミニターボ発動時に一発再生）
   sonicBoomEmitter_ = player_->AddComponent<GameEngine::ParticleEmitterComponent>();
   if (sonicBoomEmitter_) {
	  using Config = GameEngine::ParticleEmitterComponent::AttachmentConfig;
	  Config cfg;
	  cfg.followPosition = true;
	  cfg.followRotation = true;
	  cfg.followScale = true;
	  cfg.positionOffset = { 0.0f, 0.0f, 1.0f };
	  cfg.simulationSpace = Config::Space::World;
	  // AddSlot は内部で LoadSlot を呼ぶため、スロット取得後に loop=false を設定してから
	  // LoadSlot を再実行して SetLooping(false) を正しく反映させる。
	  sonicBoomSlotIndex_ = sonicBoomEmitter_->AddSlot("resources/particles/sonicBoom.json", cfg);
	  if (auto* slot = sonicBoomEmitter_->GetSlot(sonicBoomSlotIndex_)) {
		 slot->loop = false;
		 slot->autoPlay = false;
		 // loop / autoPlay を設定した上で LoadSlot を再実行する
		 sonicBoomEmitter_->LoadSlot(*slot);
	  }
   }

   // --- 仮想カメラのセットアップ ---
   rearFollowVcam_ = std::make_unique<VirtualCamera>();
   rearFollowVcam_->Initialize();
   rearFollowVcam_->SetName("PlayerRearFollowCamera");
   rearFollowVcam_->SetPriority(0);
   playerRearFollowCamera_ = rearFollowVcam_->AddComponent<PlayerRearFollowCamera>();

   mainVcam_ = std::make_unique<VirtualCamera>();
   mainVcam_->Initialize();
   mainVcam_->SetName("GravityFollowCamera");
   mainVcam_->SetPriority(-1);
   gravityFollowCamera_ = mainVcam_->AddComponent<GravityFollowCamera>();
   if (gravityFollowCamera_) {
	  gravityFollowCamera_->SetDistance(15.0f);
   }
   if (auto* brain = EngineContext::GetActiveBrain()) {
	  brain->RegisterVirtualCamera(mainVcam_.get());
   }

   leashVcam_ = std::make_unique<VirtualCamera>();
   leashVcam_->Initialize();
   leashVcam_->SetName("PlanetLeashCamera");
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
	  brain->RegisterVirtualCamera(rearFollowVcam_.get());
	  brain->RegisterVirtualCamera(leashVcam_.get());
	  brain->SetDefaultBlendTime(0.5f);
   }

   // CameraGravityBridge: gravityUp / pivotTarget をカメラへ毎フレーム通知（PlanetSwitcher が中心座標を切り替える）
   if (auto* bridge = player_->AddComponent<CameraGravityBridge>()) {
	  bridge->SetPlanetCenter(planet_->GetPosition());
	  bridge->SetPlayerRearFollowCamera(playerRearFollowCamera_);
	  bridge->SetGravityFollowCamera(gravityFollowCamera_);
	  bridge->SetPlanetLeashCamera(leashCamera_);
   }

   // VehicleController にカメラを設定（矢印キー / 右スティックの回転入力を渡す）
   if (auto* controller = player_->GetComponent<VehicleController>()) {
	  controller->SetGravityFollowCamera(cameraMode_ == CameraMode::GravityFollow ? gravityFollowCamera_ : nullptr);
   }

#ifdef USE_IMGUI
   // debugCamera_->SetPriority(100);
#endif
}

void GameTestScene::Update() {
   BaseScene::Update();

   float deltaTime = EngineContext::GetDeltaTime();
   testTime_ += deltaTime;

   // Tab キーで PlayerRearFollow / GravityFollow / PlanetLeash を切り替え
   if (EngineContext::IsKeyTriggered(KeyCode::Tab)) {
	  switch (cameraMode_) {
		 case CameraMode::PlayerRearFollow:
			cameraMode_ = CameraMode::GravityFollow;
			break;
		 case CameraMode::GravityFollow:
			cameraMode_ = CameraMode::PlanetLeash;
			break;
		 default:
			cameraMode_ = CameraMode::PlayerRearFollow;
			break;
	  }

	  if (rearFollowVcam_) {
		 rearFollowVcam_->SetPriority(cameraMode_ == CameraMode::PlayerRearFollow ? 0 : -1);
	  }

	  if (mainVcam_) {
		 mainVcam_->SetPriority(cameraMode_ == CameraMode::GravityFollow ? 0 : -1);
	  }

	  if (leashVcam_) {
		 leashVcam_->SetPriority(cameraMode_ == CameraMode::PlanetLeash ? 0 : -1);
	  }

	  // VehicleController のカメラ参照も更新
	  if (auto* controller = player_->GetComponent<VehicleController>()) {
		 controller->SetGravityFollowCamera(cameraMode_ == CameraMode::GravityFollow ? gravityFollowCamera_ : nullptr);
	  }
	  if (auto* bridge = player_->GetComponent<CameraGravityBridge>()) {
		 bridge->SetPlayerRearFollowCamera(cameraMode_ == CameraMode::PlayerRearFollow ? playerRearFollowCamera_ : nullptr);
		 bridge->SetGravityFollowCamera(cameraMode_ == CameraMode::GravityFollow ? gravityFollowCamera_ : nullptr);
		 bridge->SetPlanetLeashCamera(cameraMode_ == CameraMode::PlanetLeash ? leashCamera_ : nullptr);
	  }
   }

   // ドリフト中のみタイヤ埃パーティクルを放出（既存パーティクルの更新は常に継続）
   if (tireDustEmitter_) {
	  const auto* drift = player_->GetComponent<App::VehicleDrift>();
	  const bool isDrifting = drift && drift->IsDrifting();
	  const bool isJump = player_->GetComponent<App::CharacterJump>()->IsJumping();
	  if (isJump) {
		 for (int i = 0; i < tireDustSlotCount_; ++i) {
			if (auto* slot = tireDustEmitter_->GetSlot(i)) {
			   if (slot->particleSystem) {
				  if (auto* em = slot->particleSystem->GetEmissionModule()) {
					 em->SetEnabled(false);
				  }
			   }
			}
		 }
	  } else {
		 if (isDrifting != wasDrifting_) {
			for (int i = 0; i < tireDustSlotCount_; ++i) {
			   if (auto* slot = tireDustEmitter_->GetSlot(i)) {
				  if (slot->particleSystem) {
					 if (auto* em = slot->particleSystem->GetEmissionModule()) {
						em->SetEnabled(isDrifting);
					 }
				  }
			   }
			}
			wasDrifting_ = isDrifting;
		 }
	  }
   }

   // ミニターボ発動時にソニックブームを一発再生
   if (sonicBoomEmitter_) {
	  auto* drift = player_->GetComponent<App::VehicleDrift>();
	  if (drift && drift->ConsumeMiniTurboFired()) {
		 if (sonicBoomSlotIndex_ >= 0) {
			sonicBoomEmitter_->Play(sonicBoomSlotIndex_);
		 }
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
	  if (auto* controller = player_->GetComponent<VehicleController>()) {
		 Vector3 moveDir = controller->GetLastMoveDirection() * 2.5f;
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

#ifdef USE_IMGUI
   ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_FirstUseEver);
   ImGui::SetNextWindowSize(ImVec2(200.0f, 100.0f), ImGuiCond_FirstUseEver);
   ImGui::Begin("Scene Navigator");
   if (ImGui::Button("EngineTestScene", ImVec2(-1, 0))) {
	  EngineContext::ChangeScene("EngineTest");
   }
   ImGui::End();
   ImGui::ShowDemoWindow();
#endif 
}

void GameTestScene::Draw() {
   BaseScene::Draw();
}
