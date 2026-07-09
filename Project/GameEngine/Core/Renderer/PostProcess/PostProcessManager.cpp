#include "pch.h"
#include "PostProcessManager.h"
#include "PostProcess.h"
#include "Grayscale.h"
#include "RadialBlur.h"
#include "GaussFilter.h"
#include "Vignette.h"
#include "ChromaticAberration.h"
#include "ShockWave.h"
#include "Pixelation.h"
#include "SpeedLine.h"
#include "Bloom.h"
#include "BoxFilter.h"
#include "LinearToSRGB.h"
#include "Outline.h"
#include "AntiAliasing.h"
#include "Dissolve.h"
#include "WhiteNoise.h"
#include "Core/Renderer/Pipeline/PSOManager.h"
#include "Utility/Logger.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <algorithm>

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
   effectFactoryRegistry_.RegisterFactory("GaussFilter", [] { return std::make_unique<GaussFilter>(); });
   effectFactoryRegistry_.RegisterFactory("ChromaticAberration", [] { return std::make_unique<ChromaticAberration>(); });
   effectFactoryRegistry_.RegisterFactory("Vignette", [] { return std::make_unique<Vignette>(); });
   effectFactoryRegistry_.RegisterFactory("ShockWave", [] { return std::make_unique<ShockWave>(); });
   effectFactoryRegistry_.RegisterFactory("Pixelation", [] { return std::make_unique<Pixelation>(); });
   effectFactoryRegistry_.RegisterFactory("SpeedLine", [] { return std::make_unique<SpeedLine>(); });
   effectFactoryRegistry_.RegisterFactory("Bloom", [] { return std::make_unique<Bloom>(); });
   effectFactoryRegistry_.RegisterFactory("BoxFilter", [] { return std::make_unique<BoxFilter>(); });
   effectFactoryRegistry_.RegisterFactory("LinearToSRGB", [] { return std::make_unique<LinearToSRGB>(); });
   effectFactoryRegistry_.RegisterFactory("Outline", [] { return std::make_unique<Outline>(); });
   effectFactoryRegistry_.RegisterFactory("AntiAliasing", [] { return std::make_unique<AntiAliasing>(); });
   effectFactoryRegistry_.RegisterFactory("Dissolve", [] { return std::make_unique<Dissolve>(); });
   effectFactoryRegistry_.RegisterFactory("WhiteNoise", [] { return std::make_unique<WhiteNoise>(); });

   effectFactoriesRegistered_ = true;
}

bool PostProcessManager::LoadEffectsFromJson(const std::wstring& definitionFilePath) {
   std::string path = WStringToString(definitionFilePath);
   std::ifstream file(path);
   if (!file.is_open()) {
	  Logger::Error("[PostProcessManager] Failed to open post-process registry: " + path);
	  return false;
   }

   try {
	  json effectsJson;
	  file >> effectsJson;

	  if (!effectsJson.contains("postProcessEffects") || !effectsJson["postProcessEffects"].is_array()) {
		 Logger::Error("[PostProcessManager] Post-process registry is missing required array: postProcessEffects");
		 return false;
	  }

	  bool loadedAny = false;
	  bool allSucceeded = true;
	  size_t effectIndex = 0;
	  for (const auto& effectDef : effectsJson["postProcessEffects"]) {
		 const std::string entryLabel = path + "#postProcessEffects[" + std::to_string(effectIndex) + "]";
		 ++effectIndex;

		 if (!effectDef.is_object()) {
			Logger::Error("[PostProcessManager] Effect registry entry is not an object: " + entryLabel);
			allSucceeded = false;
			continue;
		 }

		 EffectDefinition definition;
		 definition.name = effectDef.value("name", "");
		 definition.className = effectDef.value("className", "");
		 definition.priority = effectDef.value("priority", 0);
		 definition.enabled = effectDef.value("enabled", true);
		 definition.pipelineName = effectDef.value("pipelineName", "");
		 definition.rootSignatureName = effectDef.value("rootSignatureName", "");

		 if (definition.name.empty() || definition.className.empty() ||
			definition.pipelineName.empty() || definition.rootSignatureName.empty()) {
			Logger::Error("[PostProcessManager] Effect registry entry is missing required fields: " + entryLabel);
			allSucceeded = false;
			continue;
		 }

		 // クラス名からエフェクトインスタンスを作成
		 auto effect = CreateEffectByClassName(definition.className);
		 if (!effect) {
			Logger::Error("[PostProcessManager] Unknown post-process effect class: " + definition.className);
			allSucceeded = false;
			continue;
		 }

		 effect->Initialize(device_, renderTarget_);

		 if (!ConfigureEffectPipeline(effect.get(), definition.pipelineName, definition.rootSignatureName)) {
			Logger::Error("[PostProcessManager] Failed to configure post-process effect: " + definition.name);
			allSucceeded = false;
			continue;
		 }

		 RegisterEffect(std::move(effect), definition.name, definition.priority, definition.enabled, definition.pipelineName);
		 loadedAny = true;
	  }

	  if (!loadedAny) {
		 Logger::Error("[PostProcessManager] Post-process registry did not load any effects: " + path);
		 return false;
	  }

	  return allSucceeded;
   }
   catch (const std::exception& e) {
	  Logger::Error("[PostProcessManager] Exception loading post-process registry " + path + ": " + std::string(e.what()));
	  return false;
   }
   catch (...) {
	  Logger::Error("[PostProcessManager] Unknown exception loading post-process registry: " + path);
	  return false;
   }
}

bool PostProcessManager::ConfigureEffectPipeline(PostProcess* effect, const std::string& pipelineName, const std::string& rootSignatureName) {
   if (!effect || pipelineName.empty() || !psoManager_) {
	  Logger::Error("[PostProcessManager] Invalid post-process pipeline configuration request.");
	  return false;
   }

   auto* pipeline = psoManager_->GetPipeline(pipelineName);
   auto* rootSignature = psoManager_->GetRootSignature(rootSignatureName);
   if (!pipeline || !rootSignature) {
	  Logger::Error("[PostProcessManager] Pipeline or root signature not found: pipeline=" +
		 pipelineName + ", rootSignature=" + rootSignatureName);
	  return false;
   }

   effect->SetPipeline(pipeline, rootSignature);

   const auto cbSlot = psoManager_->ResolvePipelineRootParameter(pipelineName, "constantbuffer");
   const auto inputSlot = psoManager_->ResolvePipelineRootParameter(pipelineName, "inputtexture");
   if (!cbSlot.has_value() || !inputSlot.has_value()) {
	  Logger::Error("[PostProcessManager] Required post-process binding slots are missing: pipeline=" + pipelineName);
	  return false;
   }

   const auto depthSlot = psoManager_->ResolvePipelineRootParameter(pipelineName, "depthtexture");
   const auto maskSlot = psoManager_->ResolvePipelineRootParameter(pipelineName, "masktexture");
   if (rootSignatureName == "PostProcessOutline" && !depthSlot.has_value()) {
	  Logger::Error("[PostProcessManager] Outline post-process binding slot is missing: depthtexture");
	  return false;
   }
   if (rootSignatureName == "PostProcessDissolve" && !maskSlot.has_value()) {
	  Logger::Error("[PostProcessManager] Dissolve post-process binding slot is missing: masktexture");
	  return false;
   }

   effect->SetBindingSlots(cbSlot.value(), inputSlot.value());
   if (depthSlot.has_value()) {
	  effect->SetDepthTextureRootSlot(depthSlot.value());
   }
   if (maskSlot.has_value()) {
	  effect->SetMaskTextureRootSlot(maskSlot.value());
   }
   return true;
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
