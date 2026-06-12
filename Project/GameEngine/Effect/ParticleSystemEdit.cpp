#include "pch.h"
#include "ParticleSystemEdit.h"
#include "ParticleSystem.h"
#include "Edit/ParticleMainModuleEdit.h"
#include "Edit/ParticleEmissionModuleEdit.h"
#include "Edit/ParticleShapeModuleEdit.h"
#include "Edit/ParticleLifetimeModulesEdit.h"
#include "Edit/ParticleUVModulesEdit.h"
#include "Graphics/Texture.h"
#include "Utility/VectorMath.h"
#include "Framework/EngineContext.h"
#include "VectorMath.h"

#ifdef USE_IMGUI
#include "../../../externals/imgui/imgui.h"
#include <string>
#include <map>
#include <array>
#include <vector>
#include <cstring>
#endif

namespace ParticleSystemEdit {

using namespace GameEngine;

void Edit(GameEngine::ParticleSystem* particleSystem) {
#ifdef USE_IMGUI
   if (!particleSystem) return;

   const std::string& particleSystemName = particleSystem->GetName();

   // ========================================
   // File Operations
   // ========================================
   if (ImGui::CollapsingHeader("File Operations", ImGuiTreeNodeFlags_DefaultOpen)) {
	  // name ごとにバッファを保持（初回のみ name から既定パスを生成）
	  static std::map<std::string, std::array<char, 256>> savePathBuffers;
	  static std::map<std::string, std::array<char, 256>> loadPathBuffers;

	  if (savePathBuffers.find(particleSystemName) == savePathBuffers.end()) {
		 std::string defaultPath = "resources/particles/" + particleSystemName + ".json";
		 auto& buf = savePathBuffers[particleSystemName];
		 buf.fill('\0');
		 strncpy_s(buf.data(), buf.size(), defaultPath.c_str(), buf.size() - 1);
	  }
	  if (loadPathBuffers.find(particleSystemName) == loadPathBuffers.end()) {
		 std::string defaultPath = "resources/particles/" + particleSystemName + ".json";
		 auto& buf = loadPathBuffers[particleSystemName];
		 buf.fill('\0');
		 strncpy_s(buf.data(), buf.size(), defaultPath.c_str(), buf.size() - 1);
	  }

	  auto& savePathBuffer = savePathBuffers[particleSystemName];
	  auto& loadPathBuffer = loadPathBuffers[particleSystemName];

	  ImGui::Text("Save/Load Particle Configuration");
	  ImGui::Separator();

	  // Save
	  std::string saveInputID = "##SavePath_" + particleSystemName;
	  ImGui::InputText(("Save Path" + saveInputID).c_str(), savePathBuffer.data(), savePathBuffer.size());
	  std::string saveButtonID = "Save Configuration##" + particleSystemName;
	  if (ImGui::Button(saveButtonID.c_str())) {
		 if (particleSystem->SaveToJson(savePathBuffer.data())) {
			ImGui::OpenPopup(("SaveSuccess##" + particleSystemName).c_str());
		 } else {
			ImGui::OpenPopup(("SaveFailed##" + particleSystemName).c_str());
		 }
	  }

	  // Load
	  std::string loadInputID = "##LoadPath_" + particleSystemName;
	  ImGui::InputText(("Load Path" + loadInputID).c_str(), loadPathBuffer.data(), loadPathBuffer.size());
	  std::string loadButtonID = "Load Configuration##" + particleSystemName;
	  if (ImGui::Button(loadButtonID.c_str())) {
		 if (particleSystem->LoadFromJson(loadPathBuffer.data())) {
			ImGui::OpenPopup(("LoadSuccess##" + particleSystemName).c_str());
		 } else {
			ImGui::OpenPopup(("LoadFailed##" + particleSystemName).c_str());
		 }
	  }

	  // Success/Failure Popups
	  if (ImGui::BeginPopupModal(("SaveSuccess##" + particleSystemName).c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		 ImGui::Text("Configuration saved successfully!");
		 if (ImGui::Button("OK", ImVec2(120, 0))) {
			ImGui::CloseCurrentPopup();
		 }
		 ImGui::EndPopup();
	  }

	  if (ImGui::BeginPopupModal(("SaveFailed##" + particleSystemName).c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		 ImGui::Text("Failed to save configuration.");
		 if (ImGui::Button("OK", ImVec2(120, 0))) {
			ImGui::CloseCurrentPopup();
		 }
		 ImGui::EndPopup();
	  }

	  if (ImGui::BeginPopupModal(("LoadSuccess##" + particleSystemName).c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		 ImGui::Text("Configuration loaded successfully!");
		 if (ImGui::Button("OK", ImVec2(120, 0))) {
			ImGui::CloseCurrentPopup();
		 }
		 ImGui::EndPopup();
	  }

	  if (ImGui::BeginPopupModal(("LoadFailed##" + particleSystemName).c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
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

	  std::string playButtonID = "Play##" + particleSystemName;
	  if (ImGui::Button(playButtonID.c_str())) {
		 particleSystem->Play();
	  }
	  ImGui::SameLine();
	  std::string stopButtonID = "Stop##" + particleSystemName;
	  if (ImGui::Button(stopButtonID.c_str())) {
		 particleSystem->Stop();
	  }
	  ImGui::SameLine();
	  std::string pauseButtonID = "Pause##" + particleSystemName;
	  if (ImGui::Button(pauseButtonID.c_str())) {
		 particleSystem->Pause();
	  }

	  ImGui::Separator();
   }

   // ========================================
   // Main Module
   // ========================================
   if (ImGui::CollapsingHeader("Main Module")) {
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
   // UV Transform Module
   // ========================================
   if (ImGui::CollapsingHeader("UV Transform Module")) {
	  auto* module = particleSystem->GetUVTransformModule();
	  if (module) {
		 EditUVTransformModule(module);
	  }
	  ImGui::Separator();
   }

   // ========================================
   // Texture Sheet Animation Module
   // ========================================
   if (ImGui::CollapsingHeader("Texture Sheet Animation Module")) {
	  auto* module = particleSystem->GetTextureSheetAnimationModule();
	  if (module) {
		 EditTextureSheetAnimationModule(module);
	  }
	  ImGui::Separator();
   }

   // ========================================
   // Renderer Module
   // ========================================
   if (ImGui::CollapsingHeader("Renderer Module")) {
	  auto* rendererModule = particleSystem->GetRendererModule();
	  if (rendererModule) {
		 std::string enabledCheckboxID = "Enabled##Renderer_" + particleSystemName;
		 bool enabled = rendererModule->IsEnabled();
		 if (ImGui::Checkbox(enabledCheckboxID.c_str(), &enabled)) {
			rendererModule->SetEnabled(enabled);
		 }

		 if (enabled) {
			// Rotation Space
			static const char* rotationSpaceNames[] = {
				"World",
				"Local"
			};

			int currentRotationSpace = static_cast<int>(rendererModule->GetRotationSpace());
			std::string rotationSpaceComboID = "Rotation Space##" + particleSystemName;
			if (ImGui::Combo(rotationSpaceComboID.c_str(), &currentRotationSpace, rotationSpaceNames, IM_ARRAYSIZE(rotationSpaceNames))) {
			   rendererModule->SetRotationSpace(static_cast<GameEngine::RendererModule::RotationSpace>(currentRotationSpace));
			}

			// Billboard Type
			static const char* billboardTypeNames[] = {
				"None",
				"View",
				"Horizontal",
				"Vertical",
				"Velocity"
			};

			int currentBillboardType = static_cast<int>(rendererModule->GetBillboardType());
			std::string billboardComboID = "Billboard Type##" + particleSystemName;
			if (ImGui::Combo(billboardComboID.c_str(), &currentBillboardType, billboardTypeNames, IM_ARRAYSIZE(billboardTypeNames))) {
			   rendererModule->SetBillboardType(static_cast<GameEngine::RendererModule::BillboardType>(currentBillboardType));
			}

			// Velocity Billboard Settings
			if (rendererModule->GetBillboardType() == GameEngine::RendererModule::BillboardType::Velocity) {
			   ImGui::Separator();
			   ImGui::Text("Velocity Billboard Settings:");

			   float speedScale = rendererModule->GetSpeedScale();
			   std::string speedScaleID = "Speed Scale##" + particleSystemName;
			   if (ImGui::DragFloat(speedScaleID.c_str(), &speedScale, 0.1f, 0.0f, 10.0f)) {
				  rendererModule->SetSpeedScale(speedScale);
			   }

			   float lengthScale = rendererModule->GetLengthScale();
			   std::string lengthScaleID = "Length Scale##" + particleSystemName;
			   if (ImGui::DragFloat(lengthScaleID.c_str(), &lengthScale, 0.1f, 0.0f, 10.0f)) {
				  rendererModule->SetLengthScale(lengthScale);
			   }
			}

			ImGui::Separator();

			// Particle Mesh Shape
			ImGui::Text("Particle Shape:");
			static const char* meshTypeNames[] = {
				"Quad", "Ring", "Sphere", "Box", "Cylinder",
				"Cone", "Circle", "Plane", "Torus", "Triangle"
			};
			int currentMeshType = static_cast<int>(rendererModule->GetParticleMeshType());
			std::string meshComboID = "Mesh Type##" + particleSystemName;
			if (ImGui::Combo(meshComboID.c_str(), &currentMeshType, meshTypeNames, IM_ARRAYSIZE(meshTypeNames))) {
			   rendererModule->SetParticleMeshType(static_cast<GameEngine::RendererModule::ParticleMeshType>(currentMeshType));
			}

			// Shape-specific parameters
			auto meshType = rendererModule->GetParticleMeshType();
			switch (meshType) {
			   case GameEngine::RendererModule::ParticleMeshType::Ring: {
				  float innerR = rendererModule->GetRingInnerRadius();
				  std::string id = "Inner Radius##Ring_" + particleSystemName;
				  if (ImGui::DragFloat(id.c_str(), &innerR, 0.01f, 0.0f, 10.0f))
					 rendererModule->SetRingInnerRadius(innerR);
				  float outerR = rendererModule->GetRingOuterRadius();
				  id = "Outer Radius##Ring_" + particleSystemName;
				  if (ImGui::DragFloat(id.c_str(), &outerR, 0.01f, 0.0f, 10.0f))
					 rendererModule->SetRingOuterRadius(outerR);
				  int segs = static_cast<int>(rendererModule->GetRingSegments());
				  id = "Segments##Ring_" + particleSystemName;
				  if (ImGui::DragInt(id.c_str(), &segs, 1, 3, 128))
					 rendererModule->SetRingSegments(static_cast<uint32_t>(segs));
				  break;
			   }
			   case GameEngine::RendererModule::ParticleMeshType::Sphere: {
				  float r = rendererModule->GetSphereRadius();
				  std::string id = "Radius##Sphere_" + particleSystemName;
				  if (ImGui::DragFloat(id.c_str(), &r, 0.01f, 0.01f, 10.0f))
					 rendererModule->SetSphereRadius(r);
				  int stacks = static_cast<int>(rendererModule->GetSphereStacks());
				  id = "Stacks##Sphere_" + particleSystemName;
				  if (ImGui::DragInt(id.c_str(), &stacks, 1, 2, 64))
					 rendererModule->SetSphereStacks(static_cast<uint32_t>(stacks));
				  int slices = static_cast<int>(rendererModule->GetSphereSlices());
				  id = "Slices##Sphere_" + particleSystemName;
				  if (ImGui::DragInt(id.c_str(), &slices, 1, 3, 128))
					 rendererModule->SetSphereSlices(static_cast<uint32_t>(slices));
				  break;
			   }
			   case GameEngine::RendererModule::ParticleMeshType::Box: {
				  GameEngine::Vector3 bs = rendererModule->GetBoxSize();
				  float size[3] = {bs.x, bs.y, bs.z};
				  std::string id = "Size##Box_" + particleSystemName;
				  if (ImGui::DragFloat3(id.c_str(), size, 0.01f, 0.01f, 10.0f))
					 rendererModule->SetBoxSize(GameEngine::Vector3(size[0], size[1], size[2]));
				  break;
			   }
			   case GameEngine::RendererModule::ParticleMeshType::Cylinder: {
				  float r = rendererModule->GetCylinderRadius();
				  std::string id = "Radius##Cyl_" + particleSystemName;
				  if (ImGui::DragFloat(id.c_str(), &r, 0.01f, 0.01f, 10.0f))
					 rendererModule->SetCylinderRadius(r);
				  float h = rendererModule->GetCylinderHeight();
				  id = "Height##Cyl_" + particleSystemName;
				  if (ImGui::DragFloat(id.c_str(), &h, 0.01f, 0.01f, 20.0f))
					 rendererModule->SetCylinderHeight(h);
				  int segs = static_cast<int>(rendererModule->GetCylinderSegments());
				  id = "Segments##Cyl_" + particleSystemName;
				  if (ImGui::DragInt(id.c_str(), &segs, 1, 3, 128))
					 rendererModule->SetCylinderSegments(static_cast<uint32_t>(segs));
				  break;
			   }
			   case GameEngine::RendererModule::ParticleMeshType::Cone: {
				  float r = rendererModule->GetConeRadius();
				  std::string id = "Radius##Cone_" + particleSystemName;
				  if (ImGui::DragFloat(id.c_str(), &r, 0.01f, 0.01f, 10.0f))
					 rendererModule->SetConeRadius(r);
				  float h = rendererModule->GetConeHeight();
				  id = "Height##Cone_" + particleSystemName;
				  if (ImGui::DragFloat(id.c_str(), &h, 0.01f, 0.01f, 20.0f))
					 rendererModule->SetConeHeight(h);
				  int segs = static_cast<int>(rendererModule->GetConeSegments());
				  id = "Segments##Cone_" + particleSystemName;
				  if (ImGui::DragInt(id.c_str(), &segs, 1, 3, 128))
					 rendererModule->SetConeSegments(static_cast<uint32_t>(segs));
				  break;
			   }
			   case GameEngine::RendererModule::ParticleMeshType::Circle: {
				  float r = rendererModule->GetCircleRadius();
				  std::string id = "Radius##Circle_" + particleSystemName;
				  if (ImGui::DragFloat(id.c_str(), &r, 0.01f, 0.01f, 10.0f))
					 rendererModule->SetCircleRadius(r);
				  int segs = static_cast<int>(rendererModule->GetCircleSegments());
				  id = "Segments##Circle_" + particleSystemName;
				  if (ImGui::DragInt(id.c_str(), &segs, 1, 3, 128))
					 rendererModule->SetCircleSegments(static_cast<uint32_t>(segs));
				  break;
			   }
			   case GameEngine::RendererModule::ParticleMeshType::Plane: {
				  float w = rendererModule->GetPlaneWidth();
				  std::string id = "Width##Plane_" + particleSystemName;
				  if (ImGui::DragFloat(id.c_str(), &w, 0.01f, 0.01f, 20.0f))
					 rendererModule->SetPlaneWidth(w);
				  float d = rendererModule->GetPlaneDepth();
				  id = "Depth##Plane_" + particleSystemName;
				  if (ImGui::DragFloat(id.c_str(), &d, 0.01f, 0.01f, 20.0f))
					 rendererModule->SetPlaneDepth(d);
				  break;
			   }
			   case GameEngine::RendererModule::ParticleMeshType::Torus: {
				  float maj = rendererModule->GetTorusMajorRadius();
				  std::string id = "Major Radius##Torus_" + particleSystemName;
				  if (ImGui::DragFloat(id.c_str(), &maj, 0.01f, 0.01f, 10.0f))
					 rendererModule->SetTorusMajorRadius(maj);
				  float min_ = rendererModule->GetTorusMinorRadius();
				  id = "Minor Radius##Torus_" + particleSystemName;
				  if (ImGui::DragFloat(id.c_str(), &min_, 0.01f, 0.01f, 10.0f))
					 rendererModule->SetTorusMinorRadius(min_);
				  int majSegs = static_cast<int>(rendererModule->GetTorusMajorSegments());
				  id = "Major Segments##Torus_" + particleSystemName;
				  if (ImGui::DragInt(id.c_str(), &majSegs, 1, 3, 128))
					 rendererModule->SetTorusMajorSegments(static_cast<uint32_t>(majSegs));
				  int minSegs = static_cast<int>(rendererModule->GetTorusMinorSegments());
				  id = "Minor Segments##Torus_" + particleSystemName;
				  if (ImGui::DragInt(id.c_str(), &minSegs, 1, 3, 128))
					 rendererModule->SetTorusMinorSegments(static_cast<uint32_t>(minSegs));
				  break;
			   }
			   default:
				  break;
			}

			ImGui::Separator();

			// Blend Mode
			ImGui::Text("Blend Mode:");
			{
			   static const char* blendModeNames[] = {
				  "Default (Additive)",
				  "None",
				  "Normal",
				  "Add",
				  "Subtract",
				  "Multiply",
				  "Screen"
			   };
			   auto currentBlend = particleSystem->GetBlendMode();
			   // -1 = Default (nullopt), 0..5 = BlendMode enum
			   int blendIndex = currentBlend.has_value() ? (static_cast<int>(currentBlend.value()) + 1) : 0;
			   std::string blendComboID = "##BlendMode_" + particleSystemName;
			   if (ImGui::Combo(blendComboID.c_str(), &blendIndex, blendModeNames, IM_ARRAYSIZE(blendModeNames))) {
				  if (blendIndex == 0) {
					 particleSystem->SetBlendMode(std::nullopt);
				  } else {
					 particleSystem->SetBlendMode(static_cast<BlendMode>(blendIndex - 1));
				  }
			   }
			}

			ImGui::Separator();

			// Post Process
			{
			   bool usePostProcess = particleSystem->GetUsePostProcess();
			   std::string ppCheckboxID = "Apply Post Process##" + particleSystemName;
			   if (ImGui::Checkbox(ppCheckboxID.c_str(), &usePostProcess)) {
				  particleSystem->SetUsePostProcess(usePostProcess);
			   }
			}

			ImGui::Separator();

			// Texture Settings
			ImGui::Text("Texture:");
			{
			   auto* currentTexture = particleSystem->GetTexture();
			   std::string currentTexName = particleSystem->GetTextureName().empty() ? "(None)" : particleSystem->GetTextureName();

			   // テクスチャ選択コンボ
			   std::vector<std::string> texNames = GameEngine::EngineContext::GetTextureNames();
			   std::string comboID = "##TextureSelect_" + particleSystemName;
			   if (ImGui::BeginCombo(comboID.c_str(), currentTexName.c_str())) {
				  bool noneSelected = particleSystem->GetTextureName().empty();
				  std::string noneID = "(None)##TexNone_" + particleSystemName;
				  if (ImGui::Selectable(noneID.c_str(), noneSelected)) {
					 particleSystem->SetTextureName("");
				  }
				  for (const auto& texName : texNames) {
					 bool selected = (particleSystem->GetTextureName() == texName);
					 std::string selID = texName + "##TexSel_" + particleSystemName;
					 if (ImGui::Selectable(selID.c_str(), selected)) {
						particleSystem->SetTextureName(texName);
					 }
					 if (selected) {
						ImGui::SetItemDefaultFocus();
					 }
				  }
				  ImGui::EndCombo();
			   }

			   // テクスチャプレビュー
			   auto* previewTex = currentTexture;
			   if (previewTex) {
				  ImGui::Text("%s (%ux%u)", previewTex->GetName().c_str(),
					 previewTex->GetWidth(), previewTex->GetHeight());

				  // アスペクト比を保ちながら最大128pxでプレビュー
				  const float kPreviewMax = 128.0f;
				  float w = static_cast<float>(previewTex->GetWidth());
				  float h = static_cast<float>(previewTex->GetHeight());
				  float scale = (w > h) ? (kPreviewMax / w) : (kPreviewMax / h);
				  ImVec2 previewSize(w * scale, h * scale);

				  ImTextureID texId = (ImTextureID)(previewTex->GetTextureSrvHandleGPU().ptr);
				  ImGui::Image(texId, previewSize);
			   } else {
				  ImGui::TextDisabled("No texture assigned");
			   }
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

	  auto* shapeModule = particleSystem->GetShapeModule();

	  // Shape Visualization
	  static bool showShape = true;
	  static Vector4 shapeColor(1.0f, 1.0f, 0.0f, 1.0f);

	  ImGui::Checkbox("Show Shape", &showShape);

	  if (showShape) {
		 ImGui::ColorEdit4("Shape Color", &shapeColor.x);

		 Vector3 center = shapeModule->GetPosition();
		 Vector3 scaleVec = shapeModule->GetScale();
		 Quaternion shapeRotation = shapeModule->GetRotationQuaternion();


		 // Shape-specific parameters
		 auto shapeType = shapeModule->GetShapeType();

		 switch (shapeType) {
			case GameEngine::ShapeModule::ShapeType::Sphere: {
			   float scaledRadius = shapeModule->GetRadius() * scaleVec.x;
			   GameEngine::EngineContext::DrawSphere(center, scaledRadius, shapeColor);
			   break;
			}

			case GameEngine::ShapeModule::ShapeType::Hemisphere: {
			   float scaledRadius = shapeModule->GetRadius() * scaleVec.x;
			   Vector3 up = RotateVector(Vector3(0.0f, 1.0f, 0.0f), shapeRotation);
			   GameEngine::EngineContext::DrawHemisphere(center, scaledRadius, up, shapeColor);
			   break;
			}

			case GameEngine::ShapeModule::ShapeType::Cone: {
			   // Coneの角度とスケールを反映
			   float angle = shapeModule->GetAngle();
			   float length = shapeModule->GetLength() * scaleVec.y;
			   float radius = std::tan(angle * std::numbers::pi_v<float> / 180.0f) * length;

			   Vector3 direction = RotateVector(Vector3(0.0f, 1.0f, 0.0f), shapeRotation);
			   GameEngine::EngineContext::DrawCone(center, radius, length, direction, shapeColor);
			   break;
			}

			case GameEngine::ShapeModule::ShapeType::Box: {
			   Vector3 boxSize = shapeModule->GetBoxSize();
			   Vector3 scaledSize(
				  boxSize.x * scaleVec.x,
				  boxSize.y * scaleVec.y,
				  boxSize.z * scaleVec.z
			   );
			   GameEngine::EngineContext::DrawBox(center, scaledSize, shapeColor);
			   break;
			}

			case GameEngine::ShapeModule::ShapeType::Circle: {
			   float scaledRadius = shapeModule->GetRadius() * scaleVec.x;
			   float arc = shapeModule->GetArc();
			   Vector3 circleNormal = RotateVector(Vector3(0.0f, 1.0f, 0.0f), shapeRotation);
			   Vector3 right = RotateVector(Vector3(1.0f, 0.0f, 0.0f), shapeRotation);
			   Vector3 forward = RotateVector(Vector3(0.0f, 0.0f, 1.0f), shapeRotation);

			   // Arcに対応した円の描画
			   if (arc >= 360.0f) {
				  // 完全な円
				  GameEngine::EngineContext::DrawCircle(center, scaledRadius, circleNormal, shapeColor);
			   } else {
				  // 円弧を線分で描画
				  const int segments = 32;
				  float angleStep = (arc * std::numbers::pi_v<float> / 180.0f) / segments;
				  float startAngle = -arc * 0.5f * std::numbers::pi_v<float> / 180.0f;

				  // 中心から放射状の線
				  for (int i = 0; i <= segments; ++i) {
					 float angle = startAngle + angleStep * i;
					 Vector3 p = center +
						right * (scaledRadius * std::cos(angle)) +
						forward * (scaledRadius * std::sin(angle));

					 if (i == 0 || i == segments) {
						// 端点は中心からの線も引く
						GameEngine::EngineContext::DrawLine(center, p, shapeColor);
					 }

					 if (i > 0) {
						// 前の点との接続
						float prevAngle = startAngle + angleStep * (i - 1);
						Vector3 prevP = center +
						   right * (scaledRadius * std::cos(prevAngle)) +
						   forward * (scaledRadius * std::sin(prevAngle));
						GameEngine::EngineContext::DrawLine(prevP, p, shapeColor);
					 }
				  }
			   }
			   break;
			}

			case GameEngine::ShapeModule::ShapeType::Edge:
			case GameEngine::ShapeModule::ShapeType::Point:
			   // 点とエッジは小さい球で表示
			   GameEngine::EngineContext::DrawSphere(center, 0.1f, shapeColor);
			   break;
		 }
	  }
   }

   #endif
}

}
