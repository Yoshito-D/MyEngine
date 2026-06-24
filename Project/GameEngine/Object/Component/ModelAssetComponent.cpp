#include "pch.h"
#include "ModelAssetComponent.h"
#include "ComponentRegistry.h"
#include "Framework/EngineContext.h"
#include "Object.h"

#ifdef USE_IMGUI
#include "imgui.h"
#endif

namespace {
   const bool kRegistered = GameEngine::ComponentRegistry::GetInstance().RegisterFactory(
      GameEngine::ModelAssetComponent::kTypeName,
      [](GameEngine::Object& o) -> GameEngine::IObjectComponent* { return o.AddComponent<GameEngine::ModelAssetComponent>(); }
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
}
#endif

} // namespace GameEngine
