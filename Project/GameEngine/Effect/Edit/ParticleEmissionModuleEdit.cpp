#include "pch.h"
#include "ParticleEmissionModuleEdit.h"
#include "Effect/Module/EmissionModule.h"

#ifdef USE_IMGUI
#include "../../../externals/imgui/imgui.h"
#endif

namespace ParticleSystemEdit {

void EditEmissionModule(GameEngine::EmissionModule* emissionModule) {
#ifdef USE_IMGUI
   if (!emissionModule) return;

   bool enabled = emissionModule->IsEnabled();
   if (ImGui::Checkbox("Enabled##Emission", &enabled)) {
	  emissionModule->SetEnabled(enabled);
   }

   if (enabled) {
	  float rateOverTime = emissionModule->GetRateOverTime();
	  if (ImGui::DragFloat("Rate over Time", &rateOverTime, 0.5f, 0.0f, 200.0f)) {
		 emissionModule->SetRateOverTime(rateOverTime);
	  }

	  float rateOverDistance = emissionModule->GetRateOverDistance();
	  if (ImGui::DragFloat("Rate over Distance", &rateOverDistance, 0.1f, 0.0f, 50.0f)) {
		 emissionModule->SetRateOverDistance(rateOverDistance);
	  }

	  ImGui::Text("Bursts: %zu", emissionModule->GetBursts().size());
	  if (ImGui::Button("Add Burst")) {
		 GameEngine::EmissionModule::Burst burst;
		 burst.time = 0.0f;
		 burst.count = 10;
		 burst.cycles = 1;
		 burst.interval = 1.0f;
		 emissionModule->AddBurst(burst);
	  }
	  ImGui::SameLine();
	  if (ImGui::Button("Clear Bursts")) {
		 emissionModule->ClearBursts();
	  }
   }
#endif
}

}
