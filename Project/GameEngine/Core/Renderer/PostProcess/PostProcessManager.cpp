#include "pch.h"
#include "PostProcessManager.h"
#include "PostProcess.h"
#include "Grayscale.h"
#include "RadialBlur.h"
#include "GaussBlur.h"
#include "Vignette.h"
#include "ChromaticAberration.h"
#include "ShockWave.h"
#include "Pixelation.h"
#include "Bloom.h"
#include "Core/Renderer/Pipeline/PSOManager.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <algorithm>
#include <array>

#ifdef USE_IMGUI
#include <imgui/imgui.h>
#endif

using json = nlohmann::json;

namespace {
// wstringからstringへの変換
std::string WStringToString(const std::wstring& wstr) {
   if (wstr.empty()) return std::string();
   int size_needed = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), NULL, 0, NULL, NULL);
   std::string strTo(size_needed, 0);
   WideCharToMultiByte(CP_UTF8, 0, &wstr[0], (int)wstr.size(), &strTo[0], size_needed, NULL, NULL);
   return strTo;
}
}

namespace GameEngine {

void PostProcessManager::Initialize(GraphicsDevice* device, OffscreenRenderTarget* renderTarget, PSOManager* psoManager) {
   device_ = device;
   renderTarget_ = renderTarget;
   psoManager_ = psoManager;
   RegisterDefaultEffectFactories();
}

void PostProcessManager::RegisterDefaultEffectFactories() {
   if (effectFactoriesRegistered_) {
	  return;
   }

   effectFactoryRegistry_.RegisterFactory("RadialBlur", [] { return std::make_unique<RadialBlur>(); });
   effectFactoryRegistry_.RegisterFactory("Grayscale", [] { return std::make_unique<Grayscale>(); });
   effectFactoryRegistry_.RegisterFactory("GaussBlur", [] { return std::make_unique<GaussBlur>(); });
   effectFactoryRegistry_.RegisterFactory("ChromaticAberration", [] { return std::make_unique<ChromaticAberration>(); });
   effectFactoryRegistry_.RegisterFactory("Vignette", [] { return std::make_unique<Vignette>(); });
   effectFactoryRegistry_.RegisterFactory("ShockWave", [] { return std::make_unique<ShockWave>(); });
   effectFactoryRegistry_.RegisterFactory("Pixelation", [] { return std::make_unique<Pixelation>(); });
   effectFactoryRegistry_.RegisterFactory("Bloom", [] { return std::make_unique<Bloom>(); });

   effectFactoriesRegistered_ = true;
}

bool PostProcessManager::LoadEffectsFromJson(const std::wstring& definitionFilePath) {
   std::string path = WStringToString(definitionFilePath);
   std::ifstream file(path);
   if (!file.is_open()) {
	  return false;
   }

   try {
	  json effectsJson;
	  file >> effectsJson;

	  if (!effectsJson.contains("postProcessEffects")) {
		 return false;
	  }

	  for (const auto& effectDef : effectsJson["postProcessEffects"]) {
		 EffectDefinition definition;
		 definition.name = effectDef["name"].get<std::string>();
		 definition.className = effectDef["className"].get<std::string>();
		 definition.priority = effectDef.value("priority", 0);
		 definition.enabled = effectDef.value("enabled", true);
		 definition.pipelineName = effectDef.value("pipelineName", "");

		 // クラス名からエフェクトインスタンスを作成
		 auto effect = CreateEffectByClassName(definition.className);
		 if (effect) {
			effect->Initialize(device_, renderTarget_);

			// パイプラインを設定
			if (!definition.pipelineName.empty() && psoManager_) {
			   auto* pipeline = psoManager_->GetPipeline(definition.pipelineName);
			   auto* rootSignature = psoManager_->GetRootSignature("PostProcess");
			   if (pipeline && rootSignature) {
				  effect->SetPipeline(pipeline, rootSignature);
                  const UINT cbSlot = psoManager_->ResolvePipelineRootParameter(definition.pipelineName, "constantbuffer").value_or(0);
				  const UINT inputSlot = psoManager_->ResolvePipelineRootParameter(definition.pipelineName, "inputtexture").value_or(1);
				  effect->SetBindingSlots(cbSlot, inputSlot);
			   }
			}

			RegisterEffect(std::move(effect), definition.name, definition.priority, definition.enabled, definition.pipelineName);
		 }
	  }

	  return true;
   }
   catch (const std::exception& e) {
	  (void)e;
	  return false;
   }
}

void PostProcessManager::RegisterPredefinedEffects() {
   struct PredefinedEffectEntry {
	  const char* className;
	  const char* displayName;
	  int priority;
	  const char* pipelineName;
   };

   static const std::array<PredefinedEffectEntry, 8> kEntries = {
	  PredefinedEffectEntry{ "RadialBlur", "Radial Blur", 10, "PostProcess_RadialBlur" },
	  PredefinedEffectEntry{ "Grayscale", "Grayscale", 20, "PostProcess_Grayscale" },
	  PredefinedEffectEntry{ "GaussBlur", "Gauss Blur", 30, "PostProcess_GaussBlur" },
	  PredefinedEffectEntry{ "ChromaticAberration", "Chromatic Aberration", 40, "PostProcess_ChromaticAberration" },
	  PredefinedEffectEntry{ "Vignette", "Vignette", 50, "PostProcess_Vignette" },
	  PredefinedEffectEntry{ "ShockWave", "Shock Wave", 60, "PostProcess_ShockWave" },
	  PredefinedEffectEntry{ "Pixelation", "Pixelation", 70, "PostProcess_Pixelation" },
	  PredefinedEffectEntry{ "Bloom", "Bloom", 80, "PostProcess_Bloom" }
   };

   for (const auto& entry : kEntries) {
	  auto effect = CreateEffectByClassName(entry.className);
	  if (!effect) {
		 continue;
	  }

	  effect->Initialize(device_, renderTarget_);

	  if (psoManager_) {
		 auto* pipeline = psoManager_->GetPipeline(entry.pipelineName);
		 auto* rootSig = psoManager_->GetRootSignature("PostProcess");
		 if (pipeline && rootSig) {
			effect->SetPipeline(pipeline, rootSig);
			const UINT cbSlot = psoManager_->ResolvePipelineRootParameter(entry.pipelineName, "constantbuffer").value_or(0);
			const UINT inputSlot = psoManager_->ResolvePipelineRootParameter(entry.pipelineName, "inputtexture").value_or(1);
			effect->SetBindingSlots(cbSlot, inputSlot);
		 }
	  }

	  RegisterEffect(std::move(effect), entry.displayName, entry.priority, false, entry.pipelineName);
   }
}

std::unique_ptr<PostProcess> PostProcessManager::CreateEffectByClassName(const std::string& className) {
   return effectFactoryRegistry_.Create(className);
}

void PostProcessManager::RegisterEffect(std::unique_ptr<PostProcess> effect, const std::string& name, int priority, bool enabled, const std::string& pipelineName) {
   effects_.emplace_back(std::move(effect), name, priority, pipelineName);
   effects_.back().enabled = enabled;
   SortEffectsByPriority();
}

void PostProcessManager::ApplyEffects(D3D12_GPU_DESCRIPTOR_HANDLE inputSRV) {
   if (effects_.empty()) {
	  return;
   }

   // 有効なエフェクトを収集
   std::vector<EffectInfo*> enabledEffects;
   for (auto& effectInfo : effects_) {
	  if (effectInfo.enabled && effectInfo.effect) {
		 enabledEffects.push_back(&effectInfo);
	  }
   }

   if (enabledEffects.empty()) {
	  return;
   }

   // 最初のエフェクトは inputSRV を入力として使用
   D3D12_GPU_DESCRIPTOR_HANDLE currentInputSRV = inputSRV;

   for (size_t i = 0; i < enabledEffects.size(); ++i) {
	  auto* effectInfo = enabledEffects[i];

	  // バッファを切り替えて次の描画先を準備
	  renderTarget_->SwapBuffers();

	  // エフェクトを適用
	  effectInfo->effect->Apply(currentInputSRV);

	  // 次のエフェクトのために、今回の出力を次の入力として設定
	  if (i < enabledEffects.size() - 1) {
		 currentInputSRV = renderTarget_->GetSRVHandleGPU();
	  }
   }
}

void PostProcessManager::SetEffectEnabled(const std::string& name, bool enabled) {
   auto it = FindEffect(name);
   if (it != effects_.end()) {
	  it->enabled = enabled;
   }
}

bool PostProcessManager::IsEffectEnabled(const std::string& name) const {
   auto it = FindEffect(name);
   return (it != effects_.end()) ? it->enabled : false;
}

void PostProcessManager::SetEffectPriority(const std::string& name, int priority) {
   auto it = FindEffect(name);
   if (it != effects_.end()) {
	  it->priority = priority;
	  SortEffectsByPriority();
   }
}

int PostProcessManager::GetEffectPriority(const std::string& name) const {
   auto it = FindEffect(name);
   return (it != effects_.end()) ? it->priority : -1;
}

PostProcess* PostProcessManager::GetEffect(const std::string& name) const {
   auto it = FindEffect(name);
   return (it != effects_.end()) ? it->effect.get() : nullptr;
}

std::vector<std::string> PostProcessManager::GetEffectNames() const {
   std::vector<std::string> names;
   names.reserve(effects_.size());

   for (const auto& effectInfo : effects_) {
	  names.push_back(effectInfo.name);
   }

   return names;
}

std::vector<const PostProcessManager::EffectInfo*> PostProcessManager::GetSortedEffects() const {
   std::vector<const EffectInfo*> sortedEffects;
   sortedEffects.reserve(effects_.size());

   for (const auto& effectInfo : effects_) {
	  sortedEffects.push_back(&effectInfo);
   }

   return sortedEffects;
}

void PostProcessManager::RemoveEffect(const std::string& name) {
   auto it = FindEffect(name);
   if (it != effects_.end()) {
	  effects_.erase(it);
   }
}

void PostProcessManager::ClearEffects() {
   effects_.clear();
}

void PostProcessManager::EnableAllEffects() {
   for (auto& effectInfo : effects_) {
	  effectInfo.enabled = true;
   }
}

void PostProcessManager::DisableAllEffects() {
   for (auto& effectInfo : effects_) {
	  effectInfo.enabled = false;
   }
}

#ifdef USE_IMGUI
void PostProcessManager::ShowImGuiControls() {
   ImGui::Begin("Post Process Manager");

   ImGui::Text("Post Process Control Panel");
   ImGui::Separator();

   // 全体制御ボタン
   if (ImGui::Button("Enable All")) {
	  EnableAllEffects();
   }
   ImGui::SameLine();
   if (ImGui::Button("Disable All")) {
	  DisableAllEffects();
   }

   ImGui::Separator();

   // 各エフェクトの制御
   for (auto& effectInfo : effects_) {
	  ImGui::PushID(effectInfo.effect.get());

	  if (ImGui::TreeNode(effectInfo.name.c_str())) {
		 // 有効/無効チェックボックス
		 ImGui::Checkbox("Enabled", &effectInfo.enabled);

		 // 優先度設定
		 int priority = effectInfo.priority;
		 if (ImGui::SliderInt("Priority", &priority, 0, 100)) {
			effectInfo.priority = priority;
			SortEffectsByPriority();
		 }

		 // エフェクト固有のパラメータ編集
		 if (effectInfo.effect) {
			effectInfo.effect->ImGuiEdit();
		 }

		 ImGui::TreePop();
	  }

	  ImGui::PopID();
   }

   ImGui::End();
}
#endif

auto PostProcessManager::FindEffect(const std::string& name) -> decltype(effects_.begin()) {
   return std::find_if(effects_.begin(), effects_.end(),
	  [&name](const EffectInfo& info) { return info.name == name; });
}

auto PostProcessManager::FindEffect(const std::string& name) const -> decltype(effects_.cbegin()) {
   return std::find_if(effects_.cbegin(), effects_.cend(),
	  [&name](const EffectInfo& info) { return info.name == name; });
}

void PostProcessManager::SortEffectsByPriority() {
   std::sort(effects_.begin(), effects_.end(),
	  [](const EffectInfo& a, const EffectInfo& b) {
		 return a.priority < b.priority;
	  });
}
}