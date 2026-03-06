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
#include "Core/Renderer/PSOManager.h"
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
   // 事前定義されたエフェクトを登録
   auto radialBlur = std::make_unique<RadialBlur>();
   radialBlur->Initialize(device_, renderTarget_);
   if (psoManager_) {
	  auto* pipeline = psoManager_->GetPipeline("PostProcess_RadialBlur");
	  auto* rootSig = psoManager_->GetRootSignature("PostProcess");
	  if (pipeline && rootSig) radialBlur->SetPipeline(pipeline, rootSig);
   }
   RegisterEffect(std::move(radialBlur), "Radial Blur", 10, false, "PostProcess_RadialBlur");

   auto grayscale = std::make_unique<Grayscale>();
   grayscale->Initialize(device_, renderTarget_);
   if (psoManager_) {
	  auto* pipeline = psoManager_->GetPipeline("PostProcess_Grayscale");
	  auto* rootSig = psoManager_->GetRootSignature("PostProcess");
	  if (pipeline && rootSig) grayscale->SetPipeline(pipeline, rootSig);
   }
   RegisterEffect(std::move(grayscale), "Grayscale", 20, false, "PostProcess_Grayscale");

   auto gaussBlur = std::make_unique<GaussBlur>();
   gaussBlur->Initialize(device_, renderTarget_);
   if (psoManager_) {
	  auto* pipeline = psoManager_->GetPipeline("PostProcess_GaussBlur");
	  auto* rootSig = psoManager_->GetRootSignature("PostProcess");
	  if (pipeline && rootSig) gaussBlur->SetPipeline(pipeline, rootSig);
   }
   RegisterEffect(std::move(gaussBlur), "Gauss Blur", 30, false, "PostProcess_GaussBlur");

   auto chromaticAberration = std::make_unique<ChromaticAberration>();
   chromaticAberration->Initialize(device_, renderTarget_);
   if (psoManager_) {
	  auto* pipeline = psoManager_->GetPipeline("PostProcess_ChromaticAberration");
	  auto* rootSig = psoManager_->GetRootSignature("PostProcess");
	  if (pipeline && rootSig) chromaticAberration->SetPipeline(pipeline, rootSig);
   }
   RegisterEffect(std::move(chromaticAberration), "Chromatic Aberration", 40, false, "PostProcess_ChromaticAberration");

   auto vignette = std::make_unique<Vignette>();
   vignette->Initialize(device_, renderTarget_);
   if (psoManager_) {
	  auto* pipeline = psoManager_->GetPipeline("PostProcess_Vignette");
	  auto* rootSig = psoManager_->GetRootSignature("PostProcess");
	  if (pipeline && rootSig) vignette->SetPipeline(pipeline, rootSig);
   }
   RegisterEffect(std::move(vignette), "Vignette", 50, false, "PostProcess_Vignette");

   auto shockWave = std::make_unique<ShockWave>();
   shockWave->Initialize(device_, renderTarget_);
   if (psoManager_) {
	  auto* pipeline = psoManager_->GetPipeline("PostProcess_ShockWave");
	  auto* rootSig = psoManager_->GetRootSignature("PostProcess");
	  if (pipeline && rootSig) shockWave->SetPipeline(pipeline, rootSig);
   }
   RegisterEffect(std::move(shockWave), "Shock Wave", 60, false, "PostProcess_ShockWave");

   auto pixelation = std::make_unique<Pixelation>();
   pixelation->Initialize(device_, renderTarget_);
   if (psoManager_) {
	  auto* pipeline = psoManager_->GetPipeline("PostProcess_Pixelation");
	  auto* rootSig = psoManager_->GetRootSignature("PostProcess");
	  if (pipeline && rootSig) pixelation->SetPipeline(pipeline, rootSig);
   }
   RegisterEffect(std::move(pixelation), "Pixelation", 70, false, "PostProcess_Pixelation");

   auto bloom = std::make_unique<Bloom>();
   bloom->Initialize(device_, renderTarget_);
   if (psoManager_) {
	  auto* pipeline = psoManager_->GetPipeline("PostProcess_Bloom");
	  auto* rootSig = psoManager_->GetRootSignature("PostProcess");
	  if (pipeline && rootSig) bloom->SetPipeline(pipeline, rootSig);
   }
   RegisterEffect(std::move(bloom), "Bloom", 80, false, "PostProcess_Bloom");
}

std::unique_ptr<PostProcess> PostProcessManager::CreateEffectByClassName(const std::string& className) {
   if (className == "RadialBlur") {
	  return std::make_unique<RadialBlur>();
   } else if (className == "Grayscale") {
	  return std::make_unique<Grayscale>();
   } else if (className == "GaussBlur") {
	  return std::make_unique<GaussBlur>();
   } else if (className == "ChromaticAberration") {
	  return std::make_unique<ChromaticAberration>();
   } else if (className == "Vignette") {
	  return std::make_unique<Vignette>();
   } else if (className == "ShockWave") {
	  return std::make_unique<ShockWave>();
   } else if (className == "Pixelation") {
	  return std::make_unique<Pixelation>();
   } else if (className == "Bloom") {
	  return std::make_unique<Bloom>();
   }
   return nullptr;
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