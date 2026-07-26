#include "pch.h"
#include "ParticleSystemEditor.h"
#include "Effect/ParticleSystem.h"
#include "Graphics/Texture.h"
#include "Utility/VectorMath.h"
#include "Framework/EngineContext.h"
#include "Utility/MathUtils/MathConstants.h"
#include "VectorMath.h"

#ifdef USE_IMGUI
#include "Utility/ImGuiHelper.h"
#include "imgui.h"
#include <string>
#include <map>
#include <array>
#include <vector>
#include <cstring>
#include <cmath>
#endif

namespace ParticleSystemEditor {

using namespace GameEngine;

#ifdef USE_IMGUI
namespace {

const char* Tr(const char* japanese, const char* english) {
   return ImGuiHelper::Localize({ japanese, english });
}

std::string StableLabel(const char* visibleLabel, const char* stableId) {
   return std::string(visibleLabel) + "###" + stableId;
}

std::string ScopedLabel(const char* visibleLabel, const std::string& stableId) {
   return std::string(visibleLabel) + "##" + stableId;
}

Vector3 TransformShapePoint(const Vector3& localPoint, const Vector3& center, const Quaternion& rotation) {
   return center + RotateVector(localPoint, rotation);
}

void DrawShapeLine(const Vector3& localA, const Vector3& localB, const Vector3& center, const Quaternion& rotation, const Vector4& color) {
   EngineContext::DrawLine(
	  TransformShapePoint(localA, center, rotation),
	  TransformShapePoint(localB, center, rotation),
	  color
   );
}

void DrawShapeEllipse(
   const Vector3& center,
   const Quaternion& rotation,
   const Vector3& localCenter,
   const Vector3& axisA,
   const Vector3& axisB,
   float radiusA,
   float radiusB,
   float startAngle,
   float endAngle,
   const Vector4& color,
   bool drawRadials
) {
   constexpr int kSegments = 48;
   const float angleSpan = endAngle - startAngle;
   Vector3 first{};
   Vector3 prev{};

   for (int i = 0; i <= kSegments; ++i) {
	  float t = static_cast<float>(i) / static_cast<float>(kSegments);
	  float angle = startAngle + angleSpan * t;
	  Vector3 localPoint = localCenter +
		 axisA * (radiusA * std::cos(angle)) +
		 axisB * (radiusB * std::sin(angle));

	  if (i == 0) {
		 first = localPoint;
	  } else {
		 DrawShapeLine(prev, localPoint, center, rotation, color);
	  }
	  prev = localPoint;
   }

   if (drawRadials) {
	  DrawShapeLine(localCenter, first, center, rotation, color);
	  DrawShapeLine(localCenter, prev, center, rotation, color);
   }
}

void DrawShapeEllipsoid(const Vector3& center, const Quaternion& rotation, const Vector3& scale, float radius, const Vector4& color) {
   const float rx = radius * scale.x;
   const float ry = radius * scale.y;
   const float rz = radius * scale.z;
   DrawShapeEllipse(center, rotation, Vector3(0.0f, 0.0f, 0.0f), Vector3(1.0f, 0.0f, 0.0f), Vector3(0.0f, 1.0f, 0.0f), rx, ry, 0.0f, GameEngine::MathConstants::kTwoPi, color, false);
   DrawShapeEllipse(center, rotation, Vector3(0.0f, 0.0f, 0.0f), Vector3(1.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, 1.0f), rx, rz, 0.0f, GameEngine::MathConstants::kTwoPi, color, false);
   DrawShapeEllipse(center, rotation, Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f, 1.0f, 0.0f), Vector3(0.0f, 0.0f, 1.0f), ry, rz, 0.0f, GameEngine::MathConstants::kTwoPi, color, false);
}

void DrawShapeHemisphere(const Vector3& center, const Quaternion& rotation, const Vector3& scale, float radius, const Vector4& color) {
   constexpr int kRings = 5;
   constexpr int kMeridians = 8;
   constexpr int kSegments = 16;

   for (int ring = 1; ring <= kRings; ++ring) {
	  float phi = GameEngine::MathConstants::kHalfPi * static_cast<float>(ring) / static_cast<float>(kRings);
	  float y = radius * std::cos(phi) * scale.y;
	  float ringRadius = radius * std::sin(phi);
	  DrawShapeEllipse(center, rotation, Vector3(0.0f, y, 0.0f), Vector3(1.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, 1.0f), ringRadius * scale.x, ringRadius * scale.z, 0.0f, GameEngine::MathConstants::kTwoPi, color, false);
   }

   for (int meridian = 0; meridian < kMeridians; ++meridian) {
	  float theta = GameEngine::MathConstants::kTwoPi * static_cast<float>(meridian) / static_cast<float>(kMeridians);
	  Vector3 prev(0.0f, radius * scale.y, 0.0f);

	  for (int i = 1; i <= kSegments; ++i) {
		 float phi = GameEngine::MathConstants::kHalfPi * static_cast<float>(i) / static_cast<float>(kSegments);
		 Vector3 localPoint(
			radius * std::sin(phi) * std::cos(theta) * scale.x,
			radius * std::cos(phi) * scale.y,
			radius * std::sin(phi) * std::sin(theta) * scale.z
		 );
		 DrawShapeLine(prev, localPoint, center, rotation, color);
		 prev = localPoint;
	  }
   }
}

void DrawShapeBox(const Vector3& center, const Quaternion& rotation, const Vector3& size, const Vector3& scale, const Vector4& color) {
   const Vector3 half(size.x * scale.x * 0.5f, size.y * scale.y * 0.5f, size.z * scale.z * 0.5f);
   const Vector3 vertices[] = {
	  Vector3(-half.x, -half.y, -half.z),
	  Vector3( half.x, -half.y, -half.z),
	  Vector3( half.x,  half.y, -half.z),
	  Vector3(-half.x,  half.y, -half.z),
	  Vector3(-half.x, -half.y,  half.z),
	  Vector3( half.x, -half.y,  half.z),
	  Vector3( half.x,  half.y,  half.z),
	  Vector3(-half.x,  half.y,  half.z),
   };

   const int edges[][2] = {
	  {0, 1}, {1, 2}, {2, 3}, {3, 0},
	  {4, 5}, {5, 6}, {6, 7}, {7, 4},
	  {0, 4}, {1, 5}, {2, 6}, {3, 7}
   };

   for (const auto& edge : edges) {
	  DrawShapeLine(vertices[edge[0]], vertices[edge[1]], center, rotation, color);
   }
}

void DrawShapeCone(const Vector3& center, const Quaternion& rotation, const Vector3& scale, float angleDegrees, float length, const Vector4& color) {
   constexpr int kSideLines = 8;
   const float angle = angleDegrees * GameEngine::MathConstants::kDegreesToRadians;
   const float baseY = std::cos(angle) * length * scale.y;
   const float baseRadiusX = std::sin(angle) * length * scale.x;
   const float baseRadiusZ = std::sin(angle) * length * scale.z;
   const Vector3 localBaseCenter(0.0f, baseY, 0.0f);

   DrawShapeEllipse(center, rotation, localBaseCenter, Vector3(1.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, 1.0f), baseRadiusX, baseRadiusZ, 0.0f, GameEngine::MathConstants::kTwoPi, color, false);

   for (int i = 0; i < kSideLines; ++i) {
	  float theta = GameEngine::MathConstants::kTwoPi * static_cast<float>(i) / static_cast<float>(kSideLines);
	  Vector3 basePoint(baseRadiusX * std::cos(theta), baseY, baseRadiusZ * std::sin(theta));
	  DrawShapeLine(Vector3(0.0f, 0.0f, 0.0f), basePoint, center, rotation, color);
   }
}

} // namespace
#endif

void Edit(GameEngine::ParticleSystem* particleSystem) {
#ifdef USE_IMGUI
   if (!particleSystem) return;

   const std::string& particleSystemName = particleSystem->GetName();

   // ========================================
   // File Operations
   // ========================================
   if (ImGui::CollapsingHeader(StableLabel(Tr("ファイル操作", "File Operations"), "ParticleFileOperations").c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
	  // name ごとにバッファを保持（初回のみ name から既定パスを生成）
	  static std::map<std::string, std::array<char, 256>> savePathBuffers;
	  static std::map<std::string, std::array<char, 256>> loadPathBuffers;

	  if (savePathBuffers.find(particleSystemName) == savePathBuffers.end()) {
		 std::string defaultPath = "resources/game/particles/" + particleSystemName + ".json";
		 auto& buf = savePathBuffers[particleSystemName];
		 buf.fill('\0');
		 strncpy_s(buf.data(), buf.size(), defaultPath.c_str(), buf.size() - 1);
	  }
	  if (loadPathBuffers.find(particleSystemName) == loadPathBuffers.end()) {
		 std::string defaultPath = "resources/game/particles/" + particleSystemName + ".json";
		 auto& buf = loadPathBuffers[particleSystemName];
		 buf.fill('\0');
		 strncpy_s(buf.data(), buf.size(), defaultPath.c_str(), buf.size() - 1);
	  }

	  auto& savePathBuffer = savePathBuffers[particleSystemName];
	  auto& loadPathBuffer = loadPathBuffers[particleSystemName];

	  ImGui::Text("%s", Tr("パーティクル設定の保存 / 読み込み", "Save/Load Particle Configuration"));
	  ImGui::Separator();

	  // Save
	  std::string saveInputID = "##SavePath_" + particleSystemName;
	  ImGui::InputText((std::string(Tr("保存パス", "Save Path")) + saveInputID).c_str(), savePathBuffer.data(), savePathBuffer.size());
	  std::string saveButtonID = ScopedLabel(Tr("設定を保存", "Save Configuration"), "SaveConfiguration_" + particleSystemName);
	  if (ImGui::Button(saveButtonID.c_str())) {
		 if (particleSystem->SaveToJson(savePathBuffer.data())) {
			ImGui::OpenPopup(StableLabel(Tr("保存成功", "Save Success"), ("ParticleSaveSuccess_" + particleSystemName).c_str()).c_str());
		 } else {
			ImGui::OpenPopup(StableLabel(Tr("保存失敗", "Save Failed"), ("ParticleSaveFailed_" + particleSystemName).c_str()).c_str());
		 }
	  }

	  // Load
	  std::string loadInputID = "##LoadPath_" + particleSystemName;
	  ImGui::InputText((std::string(Tr("読み込みパス", "Load Path")) + loadInputID).c_str(), loadPathBuffer.data(), loadPathBuffer.size());
	  std::string loadButtonID = ScopedLabel(Tr("設定を読み込み", "Load Configuration"), "LoadConfiguration_" + particleSystemName);
	  if (ImGui::Button(loadButtonID.c_str())) {
		 if (particleSystem->LoadFromJson(loadPathBuffer.data())) {
			ImGui::OpenPopup(StableLabel(Tr("読み込み成功", "Load Success"), ("ParticleLoadSuccess_" + particleSystemName).c_str()).c_str());
		 } else {
			ImGui::OpenPopup(StableLabel(Tr("読み込み失敗", "Load Failed"), ("ParticleLoadFailed_" + particleSystemName).c_str()).c_str());
		 }
	  }

	  // Success/Failure Popups
	  if (ImGui::BeginPopupModal(StableLabel(Tr("保存成功", "Save Success"), ("ParticleSaveSuccess_" + particleSystemName).c_str()).c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		 ImGui::Text("%s", Tr("設定を保存しました。", "Configuration saved successfully!"));
		 if (ImGui::Button("OK", ImVec2(120, 0))) {
			ImGui::CloseCurrentPopup();
		 }
		 ImGui::EndPopup();
	  }

	  if (ImGui::BeginPopupModal(StableLabel(Tr("保存失敗", "Save Failed"), ("ParticleSaveFailed_" + particleSystemName).c_str()).c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		 ImGui::Text("%s", Tr("設定の保存に失敗しました。", "Failed to save configuration."));
		 if (ImGui::Button("OK", ImVec2(120, 0))) {
			ImGui::CloseCurrentPopup();
		 }
		 ImGui::EndPopup();
	  }

	  if (ImGui::BeginPopupModal(StableLabel(Tr("読み込み成功", "Load Success"), ("ParticleLoadSuccess_" + particleSystemName).c_str()).c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		 ImGui::Text("%s", Tr("設定を読み込みました。", "Configuration loaded successfully!"));
		 if (ImGui::Button("OK", ImVec2(120, 0))) {
			ImGui::CloseCurrentPopup();
		 }
		 ImGui::EndPopup();
	  }

	  if (ImGui::BeginPopupModal(StableLabel(Tr("読み込み失敗", "Load Failed"), ("ParticleLoadFailed_" + particleSystemName).c_str()).c_str(), nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
		 ImGui::Text("%s", Tr("設定の読み込みに失敗しました。", "Failed to load configuration."));
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
   if (ImGui::CollapsingHeader(StableLabel(Tr("再生制御", "Playback Control"), "ParticlePlaybackControl").c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
	  bool isPlaying = particleSystem->IsPlaying();
	  ImGui::Text("%s: %s",
		 Tr("状態", "Status"),
		 isPlaying ? Tr("再生中", "Playing") : Tr("停止中", "Stopped"));

	  std::string playButtonID = ScopedLabel(Tr("再生", "Play"), "Play_" + particleSystemName);
	  if (ImGui::Button(playButtonID.c_str())) {
		 particleSystem->Play();
	  }
	  ImGui::SameLine();
	  std::string stopButtonID = ScopedLabel(Tr("停止", "Stop"), "Stop_" + particleSystemName);
	  if (ImGui::Button(stopButtonID.c_str())) {
		 particleSystem->Stop();
	  }
	  ImGui::SameLine();
	  std::string pauseButtonID = ScopedLabel(Tr("一時停止", "Pause"), "Pause_" + particleSystemName);
	  if (ImGui::Button(pauseButtonID.c_str())) {
		 particleSystem->Pause();
	  }

	  ImGui::Separator();
   }

   // ========================================
   // Main Module
   // ========================================
   if (ImGui::CollapsingHeader(StableLabel(Tr("メインモジュール", "Main Module"), "ParticleMainModule").c_str())) {
	  auto* mainModule = particleSystem->GetMainModule();
	  if (mainModule) {
		 mainModule->DrawInspector();
	  }
	  ImGui::Separator();
   }

   // ========================================
   // Emission Module
   // ========================================
   if (ImGui::CollapsingHeader(StableLabel(Tr("エミッションモジュール", "Emission Module"), "ParticleEmissionModule").c_str())) {
	  auto* emissionModule = particleSystem->GetEmissionModule();
	  if (emissionModule) {
		 emissionModule->DrawInspector();
	  }
	  ImGui::Separator();
   }

   // ========================================
   // Shape Module
   // ========================================
   if (ImGui::CollapsingHeader(StableLabel(Tr("形状モジュール", "Shape Module"), "ParticleShapeModule").c_str())) {
	  auto* shapeModule = particleSystem->GetShapeModule();
	  if (shapeModule) {
		 shapeModule->DrawInspector();
	  }
	  ImGui::Separator();
   }

   // ========================================
   // Velocity over Lifetime Module
   // ========================================
   if (ImGui::CollapsingHeader(StableLabel(Tr("寿命中の速度", "Velocity over Lifetime"), "ParticleVelocityOverLifetime").c_str())) {
	  auto* module = particleSystem->GetVelocityOverLifetimeModule();
	  if (module) {
		 module->DrawInspector();
	  }
	  ImGui::Separator();
   }

   // ========================================
   // Limit Velocity over Lifetime Module
   // ========================================
   if (ImGui::CollapsingHeader(StableLabel(Tr("寿命中の速度制限", "Limit Velocity over Lifetime"), "ParticleLimitVelocityOverLifetime").c_str())) {
	  auto* module = particleSystem->GetLimitVelocityModule();
	  if (module) {
		 module->DrawInspector();
	  }
	  ImGui::Separator();
   }

   // ========================================
   // Force over Lifetime Module
   // ========================================
   if (ImGui::CollapsingHeader(StableLabel(Tr("寿命中の力", "Force over Lifetime"), "ParticleForceOverLifetime").c_str())) {
	  auto* module = particleSystem->GetForceOverLifetimeModule();
	  if (module) {
		 module->DrawInspector();
	  }
	  ImGui::Separator();
   }

   // ========================================
   // Color over Lifetime Module
   // ========================================
   if (ImGui::CollapsingHeader(StableLabel(Tr("寿命中の色", "Color over Lifetime"), "ParticleColorOverLifetime").c_str())) {
	  auto* module = particleSystem->GetColorOverLifetimeModule();
	  if (module) {
		 module->DrawInspector();
	  }
	  ImGui::Separator();
   }

   // ========================================
   // Size over Lifetime Module
   // ========================================
   if (ImGui::CollapsingHeader(StableLabel(Tr("寿命中のサイズ", "Size over Lifetime"), "ParticleSizeOverLifetime").c_str())) {
	  auto* module = particleSystem->GetSizeOverLifetimeModule();
	  if (module) {
		 module->DrawInspector();
	  }
	  ImGui::Separator();
   }

   // ========================================
   // Rotation over Lifetime Module
   // ========================================
   if (ImGui::CollapsingHeader(StableLabel(Tr("寿命中の回転", "Rotation over Lifetime"), "ParticleRotationOverLifetime").c_str())) {
	  auto* module = particleSystem->GetRotationOverLifetimeModule();
	  if (module) {
		 module->DrawInspector();
	  }
	  ImGui::Separator();
   }

   // ========================================
   // Noise Module
   // ========================================
   if (ImGui::CollapsingHeader(StableLabel(Tr("ノイズモジュール", "Noise Module"), "ParticleNoiseModule").c_str())) {
	  auto* module = particleSystem->GetNoiseModule();
	  if (module) {
		 module->DrawInspector();
	  }
	  ImGui::Separator();
   }

   // ========================================
   // UV Transform Module
   // ========================================
   if (ImGui::CollapsingHeader(StableLabel(Tr("UV変換モジュール", "UV Transform Module"), "ParticleUVTransformModule").c_str())) {
	  auto* module = particleSystem->GetUVTransformModule();
	  if (module) {
		 module->DrawInspector();
	  }
	  ImGui::Separator();
   }

   // ========================================
   // Texture Sheet Animation Module
   // ========================================
   if (ImGui::CollapsingHeader(StableLabel(Tr("テクスチャシートアニメーション", "Texture Sheet Animation Module"), "ParticleTextureSheetAnimationModule").c_str())) {
	  auto* module = particleSystem->GetTextureSheetAnimationModule();
	  if (module) {
		 module->DrawInspector();
	  }
	  ImGui::Separator();
   }

   // ========================================
   // Renderer Module
   // ========================================
   if (ImGui::CollapsingHeader(StableLabel(Tr("レンダラーモジュール", "Renderer Module"), "ParticleRendererModule").c_str())) {
	  auto* rendererModule = particleSystem->GetRendererModule();
	  if (rendererModule) {
		 rendererModule->DrawInspector();

		 if (rendererModule->IsEnabled()) {
			ImGui::Separator();

			ImGui::TextDisabled("%s", Tr("GPUシミュレーション（常時有効）", "GPU Simulation (Always On)"));
			if (!particleSystem->CanUseGpuSimulation()) {
			   ImGui::TextDisabled("%s", Tr("GPUリソースを作成できないため実行できません", "GPU resources are unavailable"));
			}
		 }
	  }
	  ImGui::Separator();
   }

	if (ImGui::CollapsingHeader(StableLabel(Tr("トレイルモジュール", "Trail Module"), "ParticleTrailModule").c_str())) {
	  if (auto* trailModule = particleSystem->GetTrailModule()) {
		 trailModule->DrawInspector();
	  }
	  ImGui::Separator();
	}

	if (ImGui::CollapsingHeader(StableLabel(Tr("パーティクルメッシュモジュール", "Particle Mesh Module"), "ParticleMeshModule").c_str())) {
	  if (auto* particleMeshModule = particleSystem->GetParticleMeshModule()) {
		 particleMeshModule->DrawInspector();
	  }
	  ImGui::Separator();
	}

	if (ImGui::CollapsingHeader(StableLabel(Tr("サブエミッターモジュール", "Sub Emitter Module"), "ParticleSubEmitterModule").c_str())) {
			auto& subEmitters = particleSystem->GetSubEmitterSettings();
			ImGui::Checkbox(ScopedLabel(Tr("サブエミッター", "Sub Emitters"), "ParticleSubEmitters_" + particleSystemName).c_str(), &subEmitters.enabled);
			if (subEmitters.enabled) {
			   char deathPath[ImGuiHelper::kDefaultPathBufferSize]{};
			   std::memcpy(deathPath, subEmitters.spawnOnDeathPath.c_str(), std::min(subEmitters.spawnOnDeathPath.size(), sizeof(deathPath) - 1));
			   if (ImGui::InputText(ScopedLabel(Tr("死亡時エフェクト", "Spawn On Death"), "ParticleDeathEmitter_" + particleSystemName).c_str(), deathPath, sizeof(deathPath))) {
				  subEmitters.spawnOnDeathPath = deathPath;
			   }
			   char updatePath[ImGuiHelper::kDefaultPathBufferSize]{};
			   std::memcpy(updatePath, subEmitters.spawnOnUpdatePath.c_str(), std::min(subEmitters.spawnOnUpdatePath.size(), sizeof(updatePath) - 1));
			   if (ImGui::InputText(ScopedLabel(Tr("更新時エフェクト", "Spawn On Update"), "ParticleUpdateEmitter_" + particleSystemName).c_str(), updatePath, sizeof(updatePath))) {
				  subEmitters.spawnOnUpdatePath = updatePath;
			   }
			   char collisionPath[ImGuiHelper::kDefaultPathBufferSize]{};
			   std::memcpy(collisionPath, subEmitters.spawnOnCollisionPath.c_str(), std::min(subEmitters.spawnOnCollisionPath.size(), sizeof(collisionPath) - 1));
			   if (ImGui::InputText(ScopedLabel(Tr("衝突時エフェクト", "Spawn On Collision"), "ParticleCollisionEmitter_" + particleSystemName).c_str(), collisionPath, sizeof(collisionPath))) {
				  subEmitters.spawnOnCollisionPath = collisionPath;
			   }
			   ImGui::DragFloat(ScopedLabel(Tr("更新発生間隔", "Update Spawn Interval"), "ParticleSubInterval_" + particleSystemName).c_str(), &subEmitters.updateInterval, 0.01f, 0.001f, 60.0f);
			   int maxEvents = static_cast<int>(subEmitters.maxEventsPerFrame);
			   if (ImGui::DragInt(ScopedLabel(Tr("毎フレーム上限", "Events Per Frame"), "ParticleSubLimit_" + particleSystemName).c_str(), &maxEvents, 1.0f, 1, 1024)) {
				  subEmitters.maxEventsPerFrame = static_cast<uint32_t>(std::max(maxEvents, 1));
			   }
			   if (!subEmitters.spawnOnCollisionPath.empty()) {
				  ImGui::DragFloat3(ScopedLabel(Tr("衝突平面法線", "Collision Plane Normal"), "ParticleCollisionNormal_" + particleSystemName).c_str(), &subEmitters.collisionPlaneNormal.x, 0.01f, -1.0f, 1.0f);
				  ImGui::DragFloat(ScopedLabel(Tr("衝突平面距離", "Collision Plane Distance"), "ParticleCollisionDistance_" + particleSystemName).c_str(), &subEmitters.collisionPlaneDistance, 0.05f, -10000.0f, 10000.0f);
				  ImGui::SliderFloat(ScopedLabel(Tr("反発係数", "Restitution"), "ParticleCollisionRestitution_" + particleSystemName).c_str(), &subEmitters.collisionRestitution, 0.0f, 1.0f);
			   }
			}
			ImGui::Separator();
	}

	if (ImGui::CollapsingHeader(StableLabel(Tr("パーティクルマテリアル", "Particle Material"), "ParticleMaterialModule").c_str())) {
			// Blend Mode
			ImGui::Text("%s:", Tr("ブレンドモード", "Blend Mode"));
			{
			   const char* blendModeNames[] = {
				  Tr("デフォルト (加算)", "Default (Additive)"),
				  Tr("なし", "None"),
				  Tr("通常", "Normal"),
				  Tr("加算", "Add"),
				  Tr("減算", "Subtract"),
				  Tr("乗算", "Multiply"),
				  Tr("スクリーン", "Screen")
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

			if (auto* particleMaterial = particleSystem->GetMaterial()) {
			   float brightness = particleMaterial->GetBrightness();
			   if (ImGui::DragFloat(ScopedLabel(Tr("輝度", "Brightness"), "ParticleBrightness_" + particleSystemName).c_str(), &brightness, 0.05f, 0.0f, 100.0f)) {
				  particleMaterial->SetBrightness(brightness);
			   }

			   float alphaCutoff = particleMaterial->GetAlphaCutoff();
			   if (ImGui::SliderFloat(ScopedLabel(Tr("マスク閾値", "Alpha Cutoff"), "ParticleAlphaCutoff_" + particleSystemName).c_str(), &alphaCutoff, 0.0f, 1.0f)) {
				  particleMaterial->SetAlphaCutoff(alphaCutoff);
			   }

			   int toonSteps = static_cast<int>(particleMaterial->GetToonSteps());
			   if (ImGui::DragInt(ScopedLabel(Tr("トゥーン階調 (0=無効)", "Toon Steps (0=Off)"), "ParticleToonSteps_" + particleSystemName).c_str(), &toonSteps, 1.0f, 0, 16)) {
				  particleMaterial->SetToonSteps(static_cast<uint32_t>(std::max(toonSteps, 0)));
			   }

			   bool softParticles = particleMaterial->IsSoftParticlesEnabled();
			   if (ImGui::Checkbox(ScopedLabel(Tr("ソフトパーティクル", "Soft Particles"), "ParticleSoft_" + particleSystemName).c_str(), &softParticles)) {
				  particleMaterial->SetSoftParticlesEnabled(softParticles);
			   }
			   if (softParticles) {
				  float softDistance = particleMaterial->GetSoftParticleDistance();
				  if (ImGui::DragFloat(ScopedLabel(Tr("交差フェード距離", "Intersection Fade Distance"), "ParticleSoftDistance_" + particleSystemName).c_str(), &softDistance, 0.01f, 0.001f, 100.0f)) {
					 particleMaterial->SetSoftParticleDistance(softDistance);
				  }
			   }

			   float distortionStrength = particleMaterial->GetDistortionStrength();
			   if (ImGui::DragFloat(ScopedLabel(Tr("屈折・歪み (px)", "Refraction / Distortion (px)"), "ParticleDistortion_" + particleSystemName).c_str(), &distortionStrength, 0.1f, -100.0f, 100.0f)) {
				  particleMaterial->SetDistortionStrength(distortionStrength);
			   }
			   if (std::fabs(distortionStrength) > 0.0001f) {
				  float distortionBlend = particleMaterial->GetDistortionBlend();
				  if (ImGui::SliderFloat(ScopedLabel(Tr("歪み混合率", "Distortion Blend"), "ParticleDistortionBlend_" + particleSystemName).c_str(), &distortionBlend, 0.0f, 1.0f)) {
					 particleMaterial->SetDistortionBlend(distortionBlend);
				  }
				  bool useTextureFlow = particleMaterial->IsDistortionUsingTextureFlow();
				  if (ImGui::Checkbox(ScopedLabel(Tr("RGフローマップを使用", "Use RG Flow Map"), "ParticleDistortionFlow_" + particleSystemName).c_str(), &useTextureFlow)) {
					 particleMaterial->SetDistortionUseTextureFlow(useTextureFlow);
				  }
			   }
			}

			ImGui::Separator();
	}

	if (ImGui::CollapsingHeader(StableLabel(Tr("出力設定", "Output Settings"), "ParticleOutputSettings").c_str())) {
			// Post Process
			{
			   bool usePostProcess = particleSystem->GetUsePostProcess();
			   std::string ppCheckboxID = ScopedLabel(Tr("ポストプロセスを適用", "Apply Post Process"), "ApplyPostProcess_" + particleSystemName);
			   if (ImGui::Checkbox(ppCheckboxID.c_str(), &usePostProcess)) {
				  particleSystem->SetUsePostProcess(usePostProcess);
			   }
			}

			ImGui::Separator();
	}

	if (ImGui::CollapsingHeader(StableLabel(Tr("テクスチャとモデル", "Texture and Model"), "ParticleTextureAndModel").c_str())) {
			// Texture Settings
			ImGui::Text("%s:", Tr("テクスチャ", "Texture"));
			{
			   auto* currentTexture = particleSystem->GetTexture();
			   std::string currentTexName = particleSystem->GetTextureName().empty() ? Tr("(なし)", "(None)") : particleSystem->GetTextureName();

			   // テクスチャ選択コンボ
			   std::vector<std::string> texNames = GameEngine::EngineContext::GetTextureNames();
			   std::string comboID = "##TextureSelect_" + particleSystemName;
			   if (ImGui::BeginCombo(comboID.c_str(), currentTexName.c_str())) {
				  bool noneSelected = particleSystem->GetTextureName().empty();
				  std::string noneID = ScopedLabel(Tr("(なし)", "(None)"), "TexNone_" + particleSystemName);
				  if (ImGui::Selectable(noneID.c_str(), noneSelected)) {
					 particleSystem->SetTextureName("");
				  }
				  for (const auto& texName : texNames) {
					 if (auto* candidate = GameEngine::EngineContext::GetTexture(texName)) {
						if (candidate->GetMetadata().IsCubemap()) {
						   continue;
						}
					 }
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
				  if (previewTex->GetMetadata().IsCubemap()) {
					 ImGui::TextDisabled("%s", Tr("TextureCube はパーティクルのテクスチャ枠ではプレビューできません", "TextureCube cannot be previewed in Particle texture slot"));
				  } else {
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
				  }
			   } else {
				  ImGui::TextDisabled("%s", Tr("テクスチャ未設定", "No texture assigned"));
			   }
			}

			auto* modelAsset = particleSystem->GetModelAsset();
			if (modelAsset) {
			   ImGui::Text("%s: %s", Tr("モデル", "Model"), Tr("読み込み済み", "Loaded"));
			} else {
			   ImGui::Text("%s: %s", Tr("モデル", "Model"), Tr("なし", "None"));
			}
			ImGui::Separator();
	}

   // ========================================
   // Debug Info
   // ========================================
   if (ImGui::CollapsingHeader(StableLabel(Tr("デバッグ情報", "Debug Info"), "ParticleDebugInfo").c_str(), ImGuiTreeNodeFlags_DefaultOpen)) {
	  ImGui::Text("%s: %u / %u",
		 Tr("アクティブパーティクル", "Active Particles"),
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

	  ImGui::Checkbox(Tr("形状を表示", "Show Shape"), &showShape);

	  if (showShape) {
		 ImGui::ColorEdit4(Tr("形状カラー", "Shape Color"), &shapeColor.x);

		 Vector3 center = shapeModule->GetPosition();
		 Vector3 scaleVec = shapeModule->GetScale();
		 Quaternion shapeRotation = shapeModule->GetRotationQuaternion();

		 // Shape-specific parameters
		 auto shapeType = shapeModule->GetShapeType();

		 switch (shapeType) {
			case GameEngine::ShapeModule::ShapeType::Sphere: {
			   DrawShapeEllipsoid(center, shapeRotation, scaleVec, shapeModule->GetRadius(), shapeColor);
			   break;
			}

			case GameEngine::ShapeModule::ShapeType::Hemisphere: {
			   DrawShapeHemisphere(center, shapeRotation, scaleVec, shapeModule->GetRadius(), shapeColor);
			   break;
			}

			case GameEngine::ShapeModule::ShapeType::Cone: {
			   DrawShapeCone(center, shapeRotation, scaleVec, shapeModule->GetAngle(), shapeModule->GetLength(), shapeColor);
			   break;
			}

			case GameEngine::ShapeModule::ShapeType::Box: {
			   DrawShapeBox(center, shapeRotation, shapeModule->GetBoxSize(), scaleVec, shapeColor);
			   break;
			}

			case GameEngine::ShapeModule::ShapeType::Circle: {
			   float radiusX = shapeModule->GetRadius() * scaleVec.x;
			   float radiusZ = shapeModule->GetRadius() * scaleVec.z;
			   float arc = shapeModule->GetArc();
			   float arcRadians = arc * GameEngine::MathConstants::kDegreesToRadians;
			   if (arc >= 360.0f) {
				  DrawShapeEllipse(center, shapeRotation, Vector3(0.0f, 0.0f, 0.0f), Vector3(1.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, 1.0f), radiusX, radiusZ, 0.0f, GameEngine::MathConstants::kTwoPi, shapeColor, false);
			   } else {
				  DrawShapeEllipse(center, shapeRotation, Vector3(0.0f, 0.0f, 0.0f), Vector3(1.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, 1.0f), radiusX, radiusZ, 0.0f, arcRadians, shapeColor, true);
			   }
			   break;
			}

			case GameEngine::ShapeModule::ShapeType::Edge: {
			   DrawShapeLine(Vector3(-0.5f * scaleVec.x, 0.0f, 0.0f), Vector3(0.5f * scaleVec.x, 0.0f, 0.0f), center, shapeRotation, shapeColor);
			   break;
			}
			case GameEngine::ShapeModule::ShapeType::Point:
			   // Point emission is a single world point.
			   GameEngine::EngineContext::DrawSphere(center, 0.1f, shapeColor);
			   break;
		 }
	  }
   }

   #endif
}

}
