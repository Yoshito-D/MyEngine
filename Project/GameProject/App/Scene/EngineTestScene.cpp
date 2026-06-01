#include "EngineTestScene.h"
#include "Framework/EngineContext.h"
#include "Component/AnimationComponent.h"
#include "Component/MaterialComponent.h"

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#include "Scene/Camera/Components/OrbitalBody.h"
#include "Effect/ParticleSystemEdit.h"
#endif

using namespace GameEngine;

void EngineTestScene::Initialize() {
   BaseScene::Initialize();

   // --- アセット読み込み ---
   EngineContext::LoadModel("resources/models/cube", "AnimatedCube.gltf");
   EngineContext::LoadAnimation("resources/models/cube", "AnimatedCube.gltf");
   EngineContext::LoadTexture("resources/textures/AnimatedCube_BaseColor.png", "AnimatedCube_BaseColor");
   EngineContext::LoadTexture("resources/textures/particle.png", "particle");
   EngineContext::LoadTexture("resources/textures/gradationLine.png", "gradationLine");

   EngineContext::CreateMaterial("animCubeMaterial", 0xffffffff, 0);
   auto* animCubeMaterial = EngineContext::GetMaterial("animCubeMaterial");
   auto animCubeAsset = EngineContext::GetModel("AnimatedCube.gltf");

   // --- アニメーションキューブの作成 ---
   animCube_ = std::make_unique<Model>();
   animCube_->Create().SetModelAsset(animCubeAsset).SetMaterial(animCubeMaterial);
   animCube_->SetPosition(Vector3(3.0f, 0.0f, 0.0f));
   animCube_->SetScale(Vector3(1.0f, 1.0f, 1.0f));

   if (auto* mc = animCube_->GetComponent<MaterialComponent>()) {
	  mc->SetTextureName("AnimatedCube_BaseColor");
   }

   if (auto* anim = animCube_->AddComponent<AnimationComponent>()) {
	  anim->animationName    = "AnimatedCube.gltf";
	  anim->clipName         = "animation_AnimatedCube";
	  anim->loop             = true;
	  anim->playing          = true;
	  anim->useSkinning      = false;
	  anim->applyRotation    = true;
   }

   // --- パーティクルシステムの作成 ---
   particleSystem_ = std::make_unique<ParticleSystem>();
   particleSystem_->Create();
   particleSystem_->SetTexture(EngineContext::GetTexture("particle"));
   particleSystem_->LoadFromJson("resources/particles/hiteffect.json");
   particleSystem_->Play();

#ifdef USE_IMGUI
   // デバッグカメラを初期状態でオン（オブジェクトが正面に映る位置に配置）
   if (debugCamera_) {
      debugCamera_->SetPriority(100);
      debugCamera_->SetDistance(8.0f);
      if (auto* orbital = debugCamera_->GetOrbitalBody()) {
         orbital->SetPivotTarget(Vector3(0.0f, 0.0f, 0.0f));
      }
      isDebugCameraActive_ = true;
   }
#endif
}

void EngineTestScene::Update() {
   BaseScene::Update();

   float deltaTime = EngineContext::GetDeltaTime();

   if (particleSystem_) {
	  particleSystem_->Update(deltaTime);
	  particleSystem_->UpdateMatrix(EngineContext::GetActiveCamera());
   }

#ifdef USE_IMGUI
   ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_FirstUseEver);
   ImGui::SetNextWindowSize(ImVec2(200.0f, 100.0f), ImGuiCond_FirstUseEver);
   ImGui::Begin("Scene Navigator");
   ImGui::Text("Current: EngineTestScene");
   ImGui::Separator();
   if (ImGui::Button("Go to GameTestScene", ImVec2(-1, 0))) {
	  EngineContext::ChangeScene("GameTest");
   }
   ImGui::End();

   ParticleSystemEdit::Edit(particleSystem_.get(), "hiteffect");
#endif
}

void EngineTestScene::Draw() {
   BaseScene::Draw();

   if (particleSystem_) {
	  EngineContext::Draw(particleSystem_.get());
   }
}
