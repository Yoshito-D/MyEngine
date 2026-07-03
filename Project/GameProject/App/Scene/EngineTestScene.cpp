#include "EngineTestScene.h"
#include "Framework/EngineContext.h"
#include "Component/AnimationComponent.h"
#include "Component/MaterialComponent.h"

#ifdef USE_IMGUI
#include "imgui.h"
#include "ImGuizmo.h"
#include "Scene/Camera/Components/OrbitalBody.h"
#include "Editor/Particle/ParticleSystemEditor.h"
#endif

using namespace GameEngine;

void EngineTestScene::OnInitialize() {
   // --- アセット読み込み ---
   EngineContext::LoadModel("resources/models/cube", "AnimatedCube.gltf");
   EngineContext::LoadAnimation("resources/models/cube", "AnimatedCube.gltf");

   EngineContext::CreateMaterial("animCubeMaterial", 0xffffffff, 0);
   auto* animCubeMaterial = EngineContext::GetMaterial("animCubeMaterial");
   auto animCubeAsset = EngineContext::GetModel("AnimatedCube.gltf");

   // --- アニメーションキューブの作成 ---
   animCube_ = std::make_unique<Model>();
   animCube_->Create().SetModelAsset(animCubeAsset).SetMaterial(animCubeMaterial);
   animCube_->SetObjectName("AnimatedCube");
   animCube_->SetPosition(Vector3(3.0f, 0.0f, 0.0f));
   animCube_->SetScale(Vector3(1.0f, 1.0f, 1.0f));

   if (auto* mc = animCube_->GetComponent<MaterialComponent>()) {
	  mc->SetTextureName("AnimatedCube_BaseColor");
   }

   if (auto* anim = animCube_->AddComponent<AnimationComponent>()) {
	  anim->animationName = "AnimatedCube.gltf";
	  anim->clipName = "animation_AnimatedCube";
	  anim->loop = true;
	  anim->playing = true;
	  anim->useSkinning = false;
	  anim->applyRotation = true;
   }

   // --- パーティクルシステムの作成 ---
   particleSystem_ = std::make_unique<ParticleSystem>();
   particleSystem_->Create();
   particleSystem_->SetTexture(EngineContext::GetTexture("particle"));
   particleSystem_->SetName("bonfire");
   particleSystem_->LoadFromJson("resources/particles/bonfire.json");
   particleSystem_->Play();
}

void EngineTestScene::OnEditorUpdate() {
#ifdef USE_IMGUI
   ImGui::SetNextWindowPos(ImVec2(10.0f, 10.0f), ImGuiCond_FirstUseEver);
   ImGui::SetNextWindowSize(ImVec2(200.0f, 100.0f), ImGuiCond_FirstUseEver);
   ImGui::Begin("Scene Navigator");
   if (ImGui::Button("GameTestScene", ImVec2(-1, 0))) {
	  EngineContext::ChangeScene("GameTest");
   }
   ImGui::End();
#endif
}
