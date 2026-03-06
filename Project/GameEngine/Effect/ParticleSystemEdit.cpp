#include "pch.h"
#include "ParticleSystemEdit.h"
#include "ParticleSystem.h"
#include "Edit/ParticleMainModuleEdit.h"
#include "Edit/ParticleEmissionModuleEdit.h"
#include "Edit/ParticleShapeModuleEdit.h"
#include "Edit/ParticleLifetimeModulesEdit.h"
#include "Graphics/Texture.h"

#ifdef USE_IMGUI
#include "../../../externals/imgui/imgui.h"
#include <string>
#endif

namespace ParticleSystemEdit {

void Edit(GameEngine::ParticleSystem* particleSystem, const std::string& name) {
#ifdef USE_IMGUI
   if (!particleSystem) return;

   // ウィンドウ名を構築（重複を避けるため）
   std::string windowName = name + " Editor";
   ImGui::Begin(windowName.c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize);

   // ========================================
   // File Operations
   // ========================================
   if (ImGui::CollapsingHeader("File Operations", ImGuiTreeNodeFlags_DefaultOpen)) {
	  static char savePathBuffer[256] = "resources/particles/particle_config.json";
	  static char loadPathBuffer[256] = "resources/particles/particle_config.json";

	  ImGui::Text("Save/Load Particle Configuration");
	  ImGui::Separator();

	  // Save
	  std::string saveInputID = "##SavePath_" + name;
	  ImGui::InputText(("Save Path" + saveInputID).c_str(), savePathBuffer, IM_ARRAYSIZE(savePathBuffer));
	  std::string saveButtonID = "Save Configuration##" + name;
	  if (ImGui::Button(saveButtonID.c_str())) {
		 if (particleSystem->SaveToJson(savePathBuffer)) {
			ImGui::OpenPopup(("SaveSuccess##" + name).c_str());
		 } else {
			ImGui::OpenPopup(("SaveFailed##" + name).c_str());
		 }
	  }

	  // Load
	  std::string loadInputID = "##LoadPath_" + name;
	  ImGui::InputText(("Load Path" + loadInputID).c_str(), loadPathBuffer, IM_ARRAYSIZE(loadPathBuffer));
	  std::string loadButtonID = "Load Configuration##" + name;
	  if (ImGui::Button(loadButtonID.c_str())) {
		 if (particleSystem->LoadFromJson(loadPathBuffer)) {
			ImGui::OpenPopup(("LoadSuccess##" + name).c_str());
		 } else {
			ImGui::OpenPopup(("LoadFailed##" + name).c_str());
		 }
	  }

	  // Success/Failure Popups
	  if (ImGui::BeginPopupModal(("SaveSuccess##" + name).c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		 ImGui::Text("Configuration saved successfully!");
		 if (ImGui::Button("OK", ImVec2(120, 0))) {
			ImGui::CloseCurrentPopup();
		 }
		 ImGui::EndPopup();
	  }

	  if (ImGui::BeginPopupModal(("SaveFailed##" + name).c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		 ImGui::Text("Failed to save configuration.");
		 if (ImGui::Button("OK", ImVec2(120, 0))) {
			ImGui::CloseCurrentPopup();
		 }
		 ImGui::EndPopup();
	  }

	  if (ImGui::BeginPopupModal(("LoadSuccess##" + name).c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		 ImGui::Text("Configuration loaded successfully!");
		 if (ImGui::Button("OK", ImVec2(120, 0))) {
			ImGui::CloseCurrentPopup();
		 }
		 ImGui::EndPopup();
	  }

	  if (ImGui::BeginPopupModal(("LoadFailed##" + name).c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		 ImGui::Text("Failed to load configuration.");
		 if (ImGui::Button("OK", ImVec2(120, 0))) {
			ImGui::CloseCurrentPopup();
		 }
		 ImGui::EndPopup();
	  }

	  ImGui::Separator();
   }

   // ========================================
   // Playback Control
   // ========================================
   if (ImGui::CollapsingHeader("Playback Control", ImGuiTreeNodeFlags_DefaultOpen)) {
	  bool isPlaying = particleSystem->IsPlaying();
	  ImGui::Text("Status: %s", isPlaying ? "Playing" : "Stopped");

	  std::string playButtonID = "Play##" + name;
	  if (ImGui::Button(playButtonID.c_str())) {
		 particleSystem->Play();
	  }
	  ImGui::SameLine();
	  std::string stopButtonID = "Stop##" + name;
	  if (ImGui::Button(stopButtonID.c_str())) {
		 particleSystem->Stop();
	  }
	  ImGui::SameLine();
	  std::string pauseButtonID = "Pause##" + name;
	  if (ImGui::Button(pauseButtonID.c_str())) {
		 particleSystem->Pause();
	  }

	  ImGui::Separator();
   }

   // ========================================
   // Main Module
   // ========================================
   if (ImGui::CollapsingHeader("Main Module", ImGuiTreeNodeFlags_DefaultOpen)) {
	  auto* mainModule = particleSystem->GetMainModule();
	  if (mainModule) {
		 EditMainModule(mainModule);
	  }
	  ImGui::Separator();
   }

   // ========================================
   // Emission Module
   // ========================================
   if (ImGui::CollapsingHeader("Emission Module")) {
	  auto* emissionModule = particleSystem->GetEmissionModule();
	  if (emissionModule) {
		 EditEmissionModule(emissionModule);
	  }
	  ImGui::Separator();
   }

   // ========================================
   // Shape Module
   // ========================================
   if (ImGui::CollapsingHeader("Shape Module")) {
	  auto* shapeModule = particleSystem->GetShapeModule();
	  if (shapeModule) {
		 EditShapeModule(shapeModule);
	  }
	  ImGui::Separator();
   }

   // ========================================
   // Velocity over Lifetime Module
   // ========================================
   if (ImGui::CollapsingHeader("Velocity over Lifetime")) {
	  auto* module = particleSystem->GetVelocityOverLifetimeModule();
	  if (module) {
		 EditVelocityOverLifetimeModule(module);
	  }
	  ImGui::Separator();
   }

   // ========================================
   // Limit Velocity over Lifetime Module
   // ========================================
   if (ImGui::CollapsingHeader("Limit Velocity over Lifetime")) {
	  auto* module = particleSystem->GetLimitVelocityModule();
	  if (module) {
		 EditLimitVelocityModule(module);
	  }
	  ImGui::Separator();
   }

   // ========================================
   // Force over Lifetime Module
   // ========================================
   if (ImGui::CollapsingHeader("Force over Lifetime")) {
	  auto* module = particleSystem->GetForceOverLifetimeModule();
	  if (module) {
		 EditForceOverLifetimeModule(module);
	  }
	  ImGui::Separator();
   }

   // ========================================
   // Color over Lifetime Module
   // ========================================
   if (ImGui::CollapsingHeader("Color over Lifetime")) {
	  auto* module = particleSystem->GetColorOverLifetimeModule();
	  if (module) {
		 EditColorOverLifetimeModule(module);
	  }
	  ImGui::Separator();
   }

   // ========================================
   // Size over Lifetime Module
   // ========================================
   if (ImGui::CollapsingHeader("Size over Lifetime")) {
	  auto* module = particleSystem->GetSizeOverLifetimeModule();
	  if (module) {
		 EditSizeOverLifetimeModule(module);
	  }
	  ImGui::Separator();
   }

   // ========================================
   // Rotation over Lifetime Module
   // ========================================
   if (ImGui::CollapsingHeader("Rotation over Lifetime")) {
	  auto* module = particleSystem->GetRotationOverLifetimeModule();
	  if (module) {
		 EditRotationOverLifetimeModule(module);
	  }
	  ImGui::Separator();
   }

   // ========================================
   // Noise Module
   // ========================================
   if (ImGui::CollapsingHeader("Noise Module")) {
	  auto* module = particleSystem->GetNoiseModule();
	  if (module) {
		 EditNoiseModule(module);
	  }
	  ImGui::Separator();
   }

   // ========================================
   // Renderer Module
   // ========================================
   if (ImGui::CollapsingHeader("Renderer Module")) {
	  auto* rendererModule = particleSystem->GetRendererModule();
	  if (rendererModule) {
		 std::string enabledCheckboxID = "Enabled##Renderer_" + name;
		 bool enabled = rendererModule->IsEnabled();
		 if (ImGui::Checkbox(enabledCheckboxID.c_str(), &enabled)) {
			rendererModule->SetEnabled(enabled);
		 }

		 if (enabled) {
			// Billboard Type
			static const char* billboardTypeNames[] = {
				"Billboard",
				"Stretched Billboard",
				"Horizontal Billboard",
				"Vertical Billboard",
				"Mesh"
			};

			int currentBillboardType = static_cast<int>(rendererModule->GetBillboardType());
			std::string billboardComboID = "Billboard Type##" + name;
			if (ImGui::Combo(billboardComboID.c_str(), &currentBillboardType, billboardTypeNames, IM_ARRAYSIZE(billboardTypeNames))) {
			   rendererModule->SetBillboardType(static_cast<GameEngine::RendererModule::BillboardType>(currentBillboardType));
			}

			// Stretched Billboard Settings
			if (rendererModule->GetBillboardType() == GameEngine::RendererModule::BillboardType::StretchedBillboard) {
			   ImGui::Separator();
			   ImGui::Text("Stretched Billboard Settings:");

			   float speedScale = rendererModule->GetSpeedScale();
			   std::string speedScaleID = "Speed Scale##" + name;
			   if (ImGui::DragFloat(speedScaleID.c_str(), &speedScale, 0.1f, 0.0f, 10.0f)) {
				  rendererModule->SetSpeedScale(speedScale);
			   }

			   float lengthScale = rendererModule->GetLengthScale();
			   std::string lengthScaleID = "Length Scale##" + name;
			   if (ImGui::DragFloat(lengthScaleID.c_str(), &lengthScale, 0.1f, 0.0f, 10.0f)) {
				  rendererModule->SetLengthScale(lengthScale);
			   }
			}

			ImGui::Separator();

			// Texture and Model Settings
			ImGui::Text("Assets:");
			auto* texture = particleSystem->GetTexture();
			if (texture) {
			   ImGui::Text("Texture: %s", texture->GetName().c_str());
			} else {
			   ImGui::Text("Texture: None");
			}

			auto* modelAsset = particleSystem->GetModelAsset();
			if (modelAsset) {
			   ImGui::Text("Model: Loaded");
			} else {
			   ImGui::Text("Model: None");
			}
		 }
	  }
	  ImGui::Separator();
   }

   // ========================================
   // Debug Info
   // ========================================
   if (ImGui::CollapsingHeader("Debug Info", ImGuiTreeNodeFlags_DefaultOpen)) {
	  ImGui::Text("Active Particles: %u / %u",
		 particleSystem->GetActiveParticleCount(),
		 GameEngine::ParticleSystem::kMaxParticles);

	  ImGui::ProgressBar(
		 static_cast<float>(particleSystem->GetActiveParticleCount()) /
		 static_cast<float>(GameEngine::ParticleSystem::kMaxParticles),
		 ImVec2(0.0f, 0.0f)
	  );

	  ImGui::Separator();
   }

   ImGui::End();
#endif
}

}
