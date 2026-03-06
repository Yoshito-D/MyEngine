#include "pch.h"
#include "ParticleMainModuleEdit.h"
#include "Effect/Module/MainModule.h"
#include "Utility/VectorMath.h"

#ifdef USE_IMGUI
#include "../../../externals/imgui/imgui.h"
#endif

using namespace GameEngine;

namespace ParticleSystemEdit {

void EditMainModule(GameEngine::MainModule* mainModule) {
#ifdef USE_IMGUI
   if (!mainModule) return;

   // Duration & Looping
   float duration = mainModule->GetDuration();
   if (ImGui::DragFloat("Duration", &duration, 0.1f, 0.1f, 60.0f)) {
	  mainModule->SetDuration(duration);
   }

   bool looping = mainModule->IsLooping();
   if (ImGui::Checkbox("Looping", &looping)) {
	  mainModule->SetLooping(looping);
   }

   ImGui::Separator();

   // Start Lifetime (ランダム対応)
   ImGui::Text("Start Lifetime");
   auto& lifetime = mainModule->GetStartLifetime();
   bool lifetimeRandomize = lifetime.randomize;
   if (ImGui::Checkbox("Randomize##Lifetime", &lifetimeRandomize)) {
	  mainModule->SetStartLifetimeRandomize(lifetimeRandomize);
   }

   if (lifetimeRandomize) {
	  float lifetimeMin = lifetime.minValue;
	  float lifetimeMax = lifetime.maxValue;
	  if (ImGui::DragFloat("Min##Lifetime", &lifetimeMin, 0.1f, 0.1f, 20.0f)) {
		 // Minがmaxを超えないように制約
		 if (lifetimeMin > lifetimeMax) {
			lifetimeMin = lifetimeMax;
		 }
		 mainModule->SetStartLifetimeMin(lifetimeMin);
	  }
	  if (ImGui::DragFloat("Max##Lifetime", &lifetimeMax, 0.1f, 0.1f, 20.0f)) {
		 // MaxがMinを下回らないように制約
		 if (lifetimeMax < lifetimeMin) {
			lifetimeMax = lifetimeMin;
		 }
		 mainModule->SetStartLifetimeMax(lifetimeMax);
	  }
   } else {
	  float lifetimeValue = lifetime.minValue;
	  if (ImGui::DragFloat("Value##Lifetime", &lifetimeValue, 0.1f, 0.1f, 20.0f)) {
		 mainModule->SetStartLifetimeMin(lifetimeValue);
		 mainModule->SetStartLifetimeMax(lifetimeValue);
	  }
   }

   ImGui::Separator();

   // Start Speed (ランダム対応)
   ImGui::Text("Start Speed");
   auto& speed = mainModule->GetStartSpeed();
   bool speedRandomize = speed.randomize;
   if (ImGui::Checkbox("Randomize##Speed", &speedRandomize)) {
	  mainModule->SetStartSpeedRandomize(speedRandomize);
   }

   if (speedRandomize) {
	  float speedMin = speed.minValue;
	  float speedMax = speed.maxValue;
	  if (ImGui::DragFloat("Min##Speed", &speedMin, 0.1f, 0.0f, 50.0f)) {
		 if (speedMin > speedMax) {
			speedMin = speedMax;
		 }
		 mainModule->SetStartSpeedMin(speedMin);
	  }
	  if (ImGui::DragFloat("Max##Speed", &speedMax, 0.1f, 0.0f, 50.0f)) {
		 if (speedMax < speedMin) {
			speedMax = speedMin;
		 }
		 mainModule->SetStartSpeedMax(speedMax);
	  }
   } else {
	  float speedValue = speed.minValue;
	  if (ImGui::DragFloat("Value##Speed", &speedValue, 0.1f, 0.0f, 50.0f)) {
		 mainModule->SetStartSpeedMin(speedValue);
		 mainModule->SetStartSpeedMax(speedValue);
	  }
   }

   ImGui::Separator();

   // Start Size (ランダム対応)
   ImGui::Text("Start Size");
   auto& size = mainModule->GetStartSize();
   bool sizeRandomize = size.randomize;
   if (ImGui::Checkbox("Randomize##Size", &sizeRandomize)) {
	  mainModule->SetStartSizeRandomize(sizeRandomize);
   }

   if (sizeRandomize) {
	  float sizeMin = size.minValue;
	  float sizeMax = size.maxValue;
	  if (ImGui::DragFloat("Min##Size", &sizeMin, 0.01f, 0.01f, 10.0f)) {
		 if (sizeMin > sizeMax) {
			sizeMin = sizeMax;
		 }
		 mainModule->SetStartSizeMin(sizeMin);
	  }
	  if (ImGui::DragFloat("Max##Size", &sizeMax, 0.01f, 0.01f, 10.0f)) {
		 if (sizeMax < sizeMin) {
			sizeMax = sizeMin;
		 }
		 mainModule->SetStartSizeMax(sizeMax);
	  }
   } else {
	  float sizeValue = size.minValue;
	  if (ImGui::DragFloat("Value##Size", &sizeValue, 0.01f, 0.01f, 10.0f)) {
		 mainModule->SetStartSizeMin(sizeValue);
		 mainModule->SetStartSizeMax(sizeValue);
	  }
   }

   ImGui::Separator();

   // Start Rotation (ランダム対応)
   ImGui::Text("Start Rotation");
   auto& rotation = mainModule->GetStartRotation();
   bool rotationRandomize = rotation.randomize;
   if (ImGui::Checkbox("Randomize##Rotation", &rotationRandomize)) {
	  mainModule->SetStartRotationRandomize(rotationRandomize);
   }

   if (rotationRandomize) {
	  float rotMin[3] = { rotation.minValue.x, rotation.minValue.y, rotation.minValue.z };
	  float rotMax[3] = { rotation.maxValue.x, rotation.maxValue.y, rotation.maxValue.z };
	  if (ImGui::DragFloat3("Min##Rotation", rotMin, 1.0f, -180.0f, 180.0f)) {
		 // 各軸でMinがMaxを超えないように制約
		 for (int i = 0; i < 3; ++i) {
			if (rotMin[i] > rotMax[i]) {
			   rotMin[i] = rotMax[i];
			}
		 }
		 mainModule->SetStartRotationMin(Vector3(rotMin[0], rotMin[1], rotMin[2]));
	  }
	  if (ImGui::DragFloat3("Max##Rotation", rotMax, 1.0f, -180.0f, 180.0f)) {
		 // 各軸でMaxがMinを下回らないように制約
		 for (int i = 0; i < 3; ++i) {
			if (rotMax[i] < rotMin[i]) {
			   rotMax[i] = rotMin[i];
			}
		 }
		 mainModule->SetStartRotationMax(Vector3(rotMax[0], rotMax[1], rotMax[2]));
	  }
   } else {
	  float rotValue[3] = { rotation.minValue.x, rotation.minValue.y, rotation.minValue.z };
	  if (ImGui::DragFloat3("Value##Rotation", rotValue, 1.0f, -180.0f, 180.0f)) {
		 Vector3 rot(rotValue[0], rotValue[1], rotValue[2]);
		 mainModule->SetStartRotationMin(rot);
		 mainModule->SetStartRotationMax(rot);
	  }
   }

   ImGui::Separator();

   // Start Color (ランダム対応)
   ImGui::Text("Start Color");
   auto& color = mainModule->GetStartColor();
   bool colorRandomize = color.randomize;
   if (ImGui::Checkbox("Randomize##Color", &colorRandomize)) {
	  mainModule->SetStartColorRandomize(colorRandomize);
   }

   if (colorRandomize) {
	  Vector4 colorMinVec = ConvertUIntToColor(color.minValue);
	  Vector4 colorMaxVec = ConvertUIntToColor(color.maxValue);

	  if (ImGui::ColorEdit4("Min Color##Color", &colorMinVec.x)) {
		 uint32_t colorMin = static_cast<uint32_t>(colorMinVec.w * 255) << 24 |
			static_cast<uint32_t>(colorMinVec.x * 255) << 16 |
			static_cast<uint32_t>(colorMinVec.y * 255) << 8 |
			static_cast<uint32_t>(colorMinVec.z * 255);
		 mainModule->SetStartColorMin(colorMin);
	  }

	  if (ImGui::ColorEdit4("Max Color##Color", &colorMaxVec.x)) {
		 uint32_t colorMax = static_cast<uint32_t>(colorMaxVec.w * 255) << 24 |
			static_cast<uint32_t>(colorMaxVec.x * 255) << 16 |
			static_cast<uint32_t>(colorMaxVec.y * 255) << 8 |
			static_cast<uint32_t>(colorMaxVec.z * 255);
		 mainModule->SetStartColorMax(colorMax);
	  }
   } else {
	  Vector4 colorValue = ConvertUIntToColor(color.minValue);
	  if (ImGui::ColorEdit4("Color##Color", &colorValue.x)) {
		 uint32_t colorVal = static_cast<uint32_t>(colorValue.w * 255) << 24 |
			static_cast<uint32_t>(colorValue.x * 255) << 16 |
			static_cast<uint32_t>(colorValue.y * 255) << 8 |
			static_cast<uint32_t>(colorValue.z * 255);
		 mainModule->SetStartColorMin(colorVal);
		 mainModule->SetStartColorMax(colorVal);
	  }
   }

   ImGui::Separator();

   // Gravity Modifier
   float gravityModifier = mainModule->GetGravityModifier();
   if (ImGui::DragFloat("Gravity Modifier", &gravityModifier, 0.1f, -10.0f, 10.0f)) {
	  mainModule->SetGravityModifier(gravityModifier);
   }

   ImGui::Separator();

   // Simulation Space
   static const char* simulationSpaceNames[] = { "World", "Local" };
   int currentSpace = static_cast<int>(mainModule->GetSimulationSpace());
   if (ImGui::Combo("Simulation Space", &currentSpace, simulationSpaceNames, IM_ARRAYSIZE(simulationSpaceNames))) {
	  mainModule->SetSimulationSpace(static_cast<GameEngine::MainModule::SimulationSpace>(currentSpace));
   }

   // Max Particles
   int maxParticles = static_cast<int>(mainModule->GetMaxParticles());
   if (ImGui::DragInt("Max Particles", &maxParticles, 1, 1, 10000)) {
	  mainModule->SetMaxParticles(static_cast<uint32_t>(maxParticles));
   }

   ImGui::Separator();

   // Emission Rate (1秒間に放出するパーティクル数)
   float emissionRate = mainModule->GetEmissionRate();
   if (ImGui::DragFloat("Emission Rate (particles/sec)", &emissionRate, 0.5f, 0.0f, 500.0f)) {
	  mainModule->SetEmissionRate(emissionRate);
   }
   ImGui::TextDisabled("Number of particles emitted per second");
#endif
}

}
