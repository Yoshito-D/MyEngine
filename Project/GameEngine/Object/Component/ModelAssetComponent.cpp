#include "pch.h"
#include "ModelAssetComponent.h"
#include "ComponentRegistry.h"
#include "Framework/EngineContext.h"
#include "Object.h"

#ifdef USE_IMGUI
#include "imgui.h"
#include "Utility/ImGuiHelper.h"
#endif

namespace {
   const bool kRegistered = GameEngine::ComponentRegistry::GetInstance().RegisterFactory(
      GameEngine::ModelAssetComponent::kTypeName,
      [](GameEngine::Object& o) -> GameEngine::IObjectComponent* { return o.AddComponent<GameEngine::ModelAssetComponent>(); },
      GameEngine::ModelAssetComponent::kDisplayName
   );
}


namespace GameEngine {

const char* ModelAssetComponent::GetTypeName() const {
   return kTypeName;
}

void ModelAssetComponent::SetModelAsset(const std::shared_ptr<ModelAsset>& modelAsset) {
   modelAsset_ = modelAsset;
   assetId_.clear();
   if (modelAsset_) {
      assetId_ = modelAsset_->GetAssetId();
   }

   skinCluster_.reset();
   if (modelAsset_ && modelAsset_->HasSkinningData()) {
	  skinCluster_ = modelAsset_->CreateSkinClusterInstance();
   }
}

bool ModelAssetComponent::SetModelAssetByAssetId(const std::string& assetId) {
   if (assetId.empty()) {
      return false;
   }

   auto modelAsset = EngineContext::LoadModelByAssetId(assetId);
   if (!modelAsset) {
      return false;
   }

   SetModelAsset(modelAsset);
   assetId_ = assetId;
   return true;
}

SkinCluster* ModelAssetComponent::GetSkinCluster() {
   if (skinCluster_) {
      return &(*skinCluster_);
   }
   if (!modelAsset_) {
      return nullptr;
   }
   return modelAsset_->GetSkinCluster();
}

const SkinCluster* ModelAssetComponent::GetSkinCluster() const {
   if (skinCluster_) {
      return &(*skinCluster_);
   }
   if (!modelAsset_) {
      return nullptr;
   }
   return modelAsset_->GetSkinCluster();
}

nlohmann::json ModelAssetComponent::Serialize() const {
   return nlohmann::json{
      { "assetId", assetId_ }
   };
}

void ModelAssetComponent::Deserialize(const nlohmann::json& data) {
   if (!data.is_object()) {
      return;
   }

   if (data.contains("assetId") && data.at("assetId").is_string()) {
      SetModelAssetByAssetId(data.at("assetId").get<std::string>());
   }
}

#ifdef USE_IMGUI
void ModelAssetComponent::DrawInspector() {
   const std::string header = MakeObjectComponentHeaderLabel(GetTypeName());
   if (!ImGui::CollapsingHeader(header.c_str())) {
      return;
   }

   ImGui::Text("%s: %s",
      ImGuiHelper::Localize({ "アセットID", "Asset ID" }),
      assetId_.empty() ? ImGuiHelper::Localize({ "なし", "None" }) : assetId_.c_str());
   ImGui::Text("%s: %s",
      ImGuiHelper::Localize({ "状態", "Status" }),
      modelAsset_ ? ImGuiHelper::Localize({ "読み込み済み", "Loaded" }) : ImGuiHelper::Localize({ "未読み込み", "Not loaded" }));

   if (modelAsset_) {
      ImGui::Text("%s: %s",
         ImGuiHelper::Localize({ "スキニング", "Skinning" }),
         modelAsset_->HasSkinningData() ? ImGuiHelper::Localize({ "あり", "Available" }) : ImGuiHelper::Localize({ "なし", "None" }));
   }

   ImGui::Spacing();
}
#endif

} // namespace GameEngine
