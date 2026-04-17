#include "pch.h"
#include "ModelAssetComponent.h"
#include "ComponentRegistry.h"
#include "Object.h"

namespace {
   const bool kRegistered = GameEngine::ComponentRegistry::GetInstance().RegisterFactory(
      GameEngine::ModelAssetComponent::kTypeName,
      [](GameEngine::Object& o) -> GameEngine::IObjectComponent* { return o.AddComponent<GameEngine::ModelAssetComponent>(); }
   );
}

#ifdef USE_IMGUI
#include "externals/imgui/imgui.h"
#endif

namespace GameEngine {

const char* ModelAssetComponent::GetTypeName() const {
   return kTypeName;
}

void ModelAssetComponent::SetModelAsset(const std::shared_ptr<ModelAsset>& modelAsset) {
   modelAsset_ = modelAsset;

   skinCluster_.reset();
   if (modelAsset_ && modelAsset_->HasSkinningData()) {
      skinCluster_ = modelAsset_->CreateSkinClusterInstance();
   }
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
   return nlohmann::json::object();
}

void ModelAssetComponent::Deserialize(const nlohmann::json&) {
}

#ifdef USE_IMGUI
void ModelAssetComponent::DrawInspector() {
   const char* assetName = modelAsset_ ? "(設定済み)" : "(未設定)";
   ImGui::Text("ModelAsset: %s", assetName);
}
#endif

} // namespace GameEngine
