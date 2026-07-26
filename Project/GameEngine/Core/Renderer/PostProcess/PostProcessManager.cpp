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
#include <unordered_map>
#include <unordered_set>

#ifdef USE_IMGUI
#include <imgui/imgui.h>
#include "Utility/ImGuiHelper.h"
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

#ifdef USE_IMGUI
const char* LocalizeEditorText(const char* japanese, const char* english) {
   return GameEngine::ImGuiHelper::Localize({ japanese, english });
}

const char* LocalizeEffectName(const std::string& id, const std::string& fallback) {
   static const std::unordered_map<std::string, GameEngine::ImGuiHelper::LocalizedText> names = {
      { "radialBlur", { "放射ブラー", "Radial Blur" } },
      { "grayscale", { "グレースケール", "Grayscale" } },
      { "gaussFilter", { "ガウシアンフィルター", "Gauss Filter" } },
      { "boxFilter", { "ボックスフィルター", "Box Filter" } },
      { "chromaticAberration", { "色収差", "Chromatic Aberration" } },
      { "vignette", { "ビネット", "Vignette" } },
      { "shockWave", { "ショックウェーブ", "Shock Wave" } },
      { "outline", { "アウトライン", "Outline" } },
      { "pixelation", { "ピクセル化", "Pixelation" } },
      { "speedLine", { "集中線", "Speed Line" } },
      { "bloom", { "ブルーム", "Bloom" } },
      { "whiteNoise", { "ホワイトノイズ", "White Noise" } },
      { "dissolve", { "ディゾルブ", "Dissolve" } },
      { "antiAliasing", { "アンチエイリアシング", "Anti Aliasing" } },
      { "linearToSrgb", { "リニアからsRGB", "Linear to sRGB" } }
   };

   const auto it = names.find(id);
   return it != names.end() ? GameEngine::ImGuiHelper::Localize(it->second) : fallback.c_str();
}

std::string StableEditorLabel(const char* japanese, const char* english, const char* id) {
   return std::string(LocalizeEditorText(japanese, english)) + "###" + id;
}
#endif
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
	  std::unordered_set<std::string> loadedEffectIds;
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
		 definition.id = effectDef.value("id", definition.className);
		 definition.priority = effectDef.value("priority", 0);
		 definition.enabled = effectDef.value("enabled", true);
		 definition.pipelineName = effectDef.value("pipelineName", "");
		 definition.rootSignatureName = effectDef.value("rootSignatureName", "");

		 if (definition.name.empty() || definition.className.empty() ||
			definition.id.empty() || definition.pipelineName.empty() || definition.rootSignatureName.empty()) {
			Logger::Error("[PostProcessManager] Effect registry entry is missing required fields: " + entryLabel);
			allSucceeded = false;
			continue;
		 }
		 if (!loadedEffectIds.insert(definition.id).second) {
			Logger::Error("[PostProcessManager] Duplicate post-process effect id: " + definition.id);
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

		 RegisterEffect(std::move(effect), definition.name, definition.priority, definition.enabled, definition.pipelineName, definition.id);
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

void PostProcessManager::RegisterEffect(std::unique_ptr<PostProcess> effect, const std::string& name, int priority, bool enabled, const std::string& pipelineName, const std::string& id) {
   effects_.emplace_back(std::move(effect), id.empty() ? name : id, name, priority, pipelineName);
   EffectInfo& effectInfo = effects_.back();
   effectInfo.enabled = enabled;
   effectInfo.defaultEnabled = enabled;
   effectInfo.defaultPriority = priority;
   effectInfo.defaultSettings = effectInfo.effect ? effectInfo.effect->SerializeSettings() : nlohmann::json::object();
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

nlohmann::json PostProcessManager::SerializeSceneState() const {
   nlohmann::json stack = nlohmann::json::array();
   for (const auto& effectInfo : effects_) {
      if (!effectInfo.effect) {
         continue;
      }
      stack.push_back(nlohmann::json{
         { "id", effectInfo.id },
         { "enabled", effectInfo.enabled },
         { "order", effectInfo.priority },
         { "settings", effectInfo.effect->SerializeSettings() }
      });
   }
   return nlohmann::json{ { "postProcessStack", std::move(stack) } };
}

bool PostProcessManager::ApplySceneState(const nlohmann::json& state) {
   if (!state.is_object()) {
      return false;
   }
   const auto stackIt = state.find("postProcessStack");
   if (stackIt == state.end()) {
      return true;
   }
   if (!stackIt->is_array()) {
      return false;
   }

   std::unordered_map<std::string, const nlohmann::json*> entriesById;
   for (const auto& entry : *stackIt) {
      if (!entry.is_object()) {
         return false;
      }
      const auto idIt = entry.find("id");
      if (idIt == entry.end() || !idIt->is_string()) {
         return false;
      }
      const std::string id = idIt->get<std::string>();
      if (id.empty() || entriesById.contains(id)) {
         return false;
      }
      if (const auto enabledIt = entry.find("enabled");
         enabledIt != entry.end() && !enabledIt->is_boolean()) {
         return false;
      }
      if (const auto orderIt = entry.find("order");
         orderIt != entry.end() && !orderIt->is_number_integer()) {
         return false;
      }
      if (const auto orderIt = entry.find("order"); orderIt != entry.end()) {
         try {
            (void)orderIt->get<int>();
         } catch (const nlohmann::json::exception&) {
            return false;
         }
      }
      if (const auto settingsIt = entry.find("settings");
         settingsIt != entry.end() && !settingsIt->is_object()) {
         return false;
      }
      entriesById.emplace(id, &entry);
   }

   std::unordered_set<std::string> appliedIds;
   for (auto& effectInfo : effects_) {
      effectInfo.enabled = effectInfo.defaultEnabled;
      effectInfo.priority = effectInfo.defaultPriority;
      if (effectInfo.effect && !effectInfo.effect->DeserializeSettings(effectInfo.defaultSettings)) {
         return false;
      }

      const auto entryIt = entriesById.find(effectInfo.id);
      if (entryIt == entriesById.end()) {
         continue;
      }
      const nlohmann::json& entry = *entryIt->second;
      appliedIds.insert(effectInfo.id);
      effectInfo.enabled = entry.value("enabled", effectInfo.defaultEnabled);
      effectInfo.priority = entry.value("order", effectInfo.defaultPriority);
      if (effectInfo.effect && entry.contains("settings") &&
         !effectInfo.effect->DeserializeSettings(entry.at("settings"))) {
         return false;
      }
   }

   for (const auto& [id, entry] : entriesById) {
      (void)entry;
      if (!appliedIds.contains(id)) {
         Logger::EngineWarning("[PostProcessManager] Scene references an unknown effect: " + id);
      }
   }
   SortEffectsByPriority();
   return true;
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
bool PostProcessManager::ShowImGuiControls() {
   const nlohmann::json stateBeforeEditing = SerializeSceneState();
   const std::string windowLabel = StableEditorLabel(
      "ポストプロセス", "Post Process Manager", "PostProcessManager");
   ImGui::Begin(windowLabel.c_str());

   ImGui::Text("%s", LocalizeEditorText("ポストプロセス設定", "Post Process Control Panel"));
   ImGui::Separator();

   // 全体制御ボタン
   if (ImGui::Button(LocalizeEditorText("すべて有効", "Enable All"))) {
	  EnableAllEffects();
   }
   ImGui::SameLine();
   if (ImGui::Button(LocalizeEditorText("すべて無効", "Disable All"))) {
	  DisableAllEffects();
   }

   ImGui::Separator();

   // 各エフェクトの制御
   bool shouldSortEffects = false;
   for (auto& effectInfo : effects_) {
	  ImGui::PushID(effectInfo.effect.get());

      const std::string effectLabel = std::string(
         LocalizeEffectName(effectInfo.id, effectInfo.name)) + "###Effect";
	  if (ImGui::TreeNode(effectLabel.c_str())) {
		 // 有効/無効チェックボックス
		 ImGui::Checkbox(LocalizeEditorText("有効", "Enabled"), &effectInfo.enabled);

		 // 優先度設定
		 int priority = effectInfo.priority;
		 if (ImGui::SliderInt(LocalizeEditorText("実行順", "Priority"), &priority, 0, 100)) {
			effectInfo.priority = priority;
			shouldSortEffects = true;
		 }

		 // エフェクト固有のパラメータ編集
		 if (effectInfo.effect) {
			effectInfo.effect->ImGuiEdit();
		 }

		 ImGui::TreePop();
	  }

	  ImGui::PopID();
   }

   if (shouldSortEffects) {
	  SortEffectsByPriority();
   }
   ImGui::End();
   return stateBeforeEditing != SerializeSceneState();
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
