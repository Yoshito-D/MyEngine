#include "pch.h"
#include "ParticleEmissionModuleEdit.h"
#include "Effect/Module/EmissionModule.h"

#ifdef USE_IMGUI
#include "imgui.h"
#endif

namespace ParticleSystemEdit {

void EditEmissionModule(GameEngine::EmissionModule* emissionModule) {
#ifdef USE_IMGUI
   if (!emissionModule) return;

   bool enabled = emissionModule->IsEnabled();
   if (ImGui::Checkbox("Enabled (有効)##Emission", &enabled)) {
	  emissionModule->SetEnabled(enabled);
   }

   if (enabled) {
	  float rateOverTime = emissionModule->GetRateOverTime();
	  if (ImGui::DragFloat("Rate over Time (時間あたり放出数)", &rateOverTime, 0.5f, 0.0f, 200.0f)) {
		 emissionModule->SetRateOverTime(rateOverTime);
	  }

	  float rateOverDistance = emissionModule->GetRateOverDistance();
	  if (ImGui::DragFloat("Rate over Distance (距離あたり放出数)", &rateOverDistance, 0.1f, 0.0f, 50.0f)) {
		 emissionModule->SetRateOverDistance(rateOverDistance);
	  }

	  ImGui::Separator();
	  ImGui::Text("Bursts (バースト) (%zu)", emissionModule->GetBursts().size());

	  auto& bursts = emissionModule->GetBursts();

	  int removeIndex = -1;
	  for (int i = 0; i < static_cast<int>(bursts.size()); ++i) {
		 auto& burst = bursts[i];
		 ImGui::PushID(i);

		 // 折りたたみヘッダーで各 Burst を管理
		 bool open = ImGui::CollapsingHeader(("Burst (バースト) [" + std::to_string(i) + "]").c_str());
		 ImGui::SameLine();
		 if (ImGui::SmallButton("Remove (削除)")) {
			removeIndex = i;
		 }

		 if (open) {
			ImGui::Indent();

			// Time
			if (ImGui::DragFloat("Time (時間)", &burst.time, 0.05f, 0.0f, 999.0f, "%.2f")) {
			   emissionModule->ResetBurstStates();
			}
			ImGui::SameLine();
			ImGui::TextDisabled("(sec) 発火開始時刻");

			// Count
			int count = static_cast<int>(burst.count);
			if (ImGui::DragInt("Count (数)", &count, 1, 1, 10000)) {
			   burst.count = static_cast<uint32_t>(std::max(count, 1));
			   emissionModule->ResetBurstStates();
			}
			ImGui::SameLine();
			ImGui::TextDisabled("1回の放出数");

			// Cycles（0 = 無限ループ）
			int cycles = static_cast<int>(burst.cycles);
			if (ImGui::DragInt("Cycles (回数)", &cycles, 1, 0, 1000)) {
			   burst.cycles = static_cast<uint32_t>(std::max(cycles, 0));
			   emissionModule->ResetBurstStates();
			}
			ImGui::SameLine();
			if (burst.cycles == 0) {
			   ImGui::TextDisabled("繰り返し回数 (0 = \xe2\x88\x9e)");
			} else {
			   ImGui::TextDisabled("繰り返し回数");
			}

			// Interval
			if (ImGui::DragFloat("Interval (間隔)", &burst.interval, 0.05f, 0.01f, 60.0f, "%.2f")) {
			   emissionModule->ResetBurstStates();
			}
			ImGui::SameLine();
			ImGui::TextDisabled("(sec) 繰り返し間隔");

			// 現在の発火状態をデバッグ表示
			ImGui::TextDisabled("  firedCount: %u  nextFireTime: %.2f",
			   burst.firedCount, burst.nextFireTime);

			ImGui::Unindent();
		 }

		 ImGui::PopID();
	  }

	  // 削除処理（ループ外で実施）
	  if (removeIndex >= 0) {
		 bursts.erase(bursts.begin() + removeIndex);
		 emissionModule->ResetBurstStates();
	  }

	  if (ImGui::Button("+ Add Burst (+ バースト追加)")) {
		 GameEngine::EmissionModule::Burst burst;
		 burst.time    = 0.0f;
		 burst.count   = 10;
		 burst.cycles  = 1;
		 burst.interval = 1.0f;
		 emissionModule->AddBurst(burst);
		 emissionModule->ResetBurstStates();
	  }
	  ImGui::SameLine();
	  if (ImGui::Button("Clear Bursts (バーストをクリア)")) {
		 emissionModule->ClearBursts();
	  }
   }
#endif
}

}
