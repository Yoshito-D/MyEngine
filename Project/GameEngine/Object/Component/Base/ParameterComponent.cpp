#include "GameEngine/pch.h"
#include "GameEngine/Object/Component/Base/ParameterComponent.h"
#include "GameEngine/Assets/ParameterAssetStore.h"
#include "GameEngine/Utility/Logger.h"
#include <stdexcept>
#include "GameEngine/Framework/EngineContext.h"

namespace GameEngine {
namespace {
void PrepareParameterDependencies(const nlohmann::json& data) {
   nlohmann::json scene;
   scene["objects"] =
      nlohmann::json::array({nlohmann::json{{"components", nlohmann::json::array({nlohmann::json{{"data", data}}})}}});
   if (!EngineContext::PreloadSceneTextures(scene)) {
      throw std::runtime_error("Could not prepare shared parameter dependencies");
   }
}
}

nlohmann::json ParameterComponent::GetSharedParameters() const {
   return nlohmann::json::object();
}

void ParameterComponent::ApplySharedParameters(const nlohmann::json&) {
   throw std::runtime_error("Component has no shared settings adapter");
}

nlohmann::json ParameterComponent::ExtractSharedParameters(std::initializer_list<const char*> excluded) const {
   auto data = Serialize();
   for (const char* key : excluded) {
      data.erase(key);
   }
   return data;
}

nlohmann::json ParameterComponent::GetSceneParameters() const {
   auto data = Serialize();
   if (!parameterAssetId_.empty()) {
      const auto shared = GetSharedParameters();
      for (auto it = shared.begin(); it != shared.end(); ++it) {
         data.erase(it.key());
      }
   }
   return data;
}

void ParameterComponent::WriteParameters(nlohmann::json& entry) const {
   entry["data"] = GetSceneParameters();
   if (!parameterAssetId_.empty()) {
      entry["parameterAssetId"] = parameterAssetId_;
   }
}

bool ParameterComponent::RestoreParameters(const nlohmann::json& entry, ParameterAssetStore& store) {
   parameterAssetId_.clear();
   parameterStore_ = &store;
   parameterRevision_ = 0;
   const auto local = entry.value("data", nlohmann::json::object());
   const auto id = entry.value("parameterAssetId", std::string{});
   if (id.empty()) {
      Deserialize(local);
      return true;
   }
   const auto* asset = store.Load(id, GetParameterDomain(), GetParameterType());
   if (!asset || !ParameterAssetStore::Validate(GetSharedParameters(), asset->data)) {
      parameterError_ = asset ? "Invalid shared parameter schema: " + id : store.GetLastError();
      Logger::Error(parameterError_);
      return false;
   }
   auto data = asset->data;
   for (auto it = local.begin(); it != local.end(); ++it) {
      if (data.contains(it.key())) {
         parameterError_ = "Shared parameter duplicated in scene: " + id + "/" + it.key();
         return false;
      }
      data[it.key()] = it.value();
   }
   Deserialize(data);
   parameterAssetId_ = id;
   parameterRevision_ = asset->revision;
   parameterValues_ = asset->data;
   parameterError_.clear();
   return true;
}

bool ParameterComponent::BindParameterAsset(ParameterAssetStore& store, const std::string& id) {
   if (id.empty()) {
      SynchronizeParameters();
      parameterAssetId_.clear();
      parameterRevision_ = 0;
      parameterStore_ = &store;
      parameterError_.clear();
      return true;
   }
   const auto* asset = store.Load(id, GetParameterDomain(), GetParameterType());
   const auto previous = GetSharedParameters();
   if (!asset || previous.empty() || !ParameterAssetStore::Validate(previous, asset->data)) {
      parameterError_ = asset ? "Invalid shared parameter schema: " + id : store.GetLastError();
      return false;
   }
   try {
      PrepareParameterDependencies(asset->data);
      ApplySharedParameters(asset->data);
   } catch (const std::exception& error) {
      parameterError_ = error.what();
      ApplySharedParameters(previous);
      return false;
   }
   parameterStore_ = &store;
   parameterAssetId_ = id;
   parameterRevision_ = asset->revision;
   parameterValues_ = asset->data;
   parameterError_.clear();
   return true;
}

void ParameterComponent::SynchronizeParameters() {
   if (!parameterStore_ || parameterAssetId_.empty()) return;

   const auto* asset = parameterStore_->Find(parameterAssetId_);
   if (!asset || asset->revision == parameterRevision_) return;

   const auto previous = GetSharedParameters();
   // 同じ失敗を毎フレーム再試行しない。次の修正revisionで再検証する。
   parameterRevision_ = asset->revision;
   try {
      if (!ParameterAssetStore::Validate(previous, asset->data)) {
         throw std::runtime_error("Invalid shared parameter schema: " + parameterAssetId_);
      }
      auto desired = previous;
      // マテリアルの一時色など、同じ配列内の別プロパティへの実行時変更も維持する。
      // 実行中に消滅するスロットなどで構造が変わっている場合は、変更された設定群だけを置換する。
      const auto sameShape = [](const auto& self, const nlohmann::json& lhs, const nlohmann::json& rhs) -> bool {
         if (lhs.type() != rhs.type()) {
            return lhs.is_number() && rhs.is_number();
         }
         if (lhs.is_array() && lhs.size() != rhs.size()) {
            return false;
         }
         if (lhs.is_structured()) {
            for (auto it = lhs.begin(); it != lhs.end(); ++it) {
               if (lhs.is_object()) {
                  if (!rhs.contains(it.key()) || !self(self, it.value(), rhs.at(it.key()))) {
                     return false;
                  }
               } else if (!self(self, it.value(), rhs.at(it - lhs.begin()))) {
                  return false;
               }
            }
         }
         return true;
      };
      if (sameShape(sameShape, parameterValues_, previous)) {
         desired = previous.patch(nlohmann::json::diff(parameterValues_, asset->data));
      } else {
         for (auto it = asset->data.begin(); it != asset->data.end(); ++it) {
            if (!parameterValues_.contains(it.key()) || parameterValues_.at(it.key()) != it.value()) {
               desired[it.key()] = it.value();
            }
         }
      }
      PrepareParameterDependencies(desired);
      ApplySharedParameters(desired);
      parameterValues_ = asset->data;
      parameterError_.clear();
   } catch (const std::exception& error) {
      parameterError_ = error.what();
      Logger::Error(parameterError_);
      try {
         ApplySharedParameters(previous);
      } catch (const std::exception& restoreError) {
         Logger::Error("Parameter rollback failed: " + std::string(restoreError.what()));
      }
   }
}

bool ParameterComponent::CommitParameterEdit(const nlohmann::json& before, bool editing) {
   if (!parameterStore_ || parameterAssetId_.empty()) {
      return true;
   }
   const auto after = GetSharedParameters();
   const auto* asset = parameterStore_->Find(parameterAssetId_);
   if (!asset) {
      return false;
   }
   try {
      // 部分的なUI編集だけを正本へ反映し、ゲームが一時変更した別の値を保存しない。
      const auto desired = asset->data.patch(nlohmann::json::diff(before, after));
      if (before != after) {
         PrepareParameterDependencies(desired);
      }
      if (!parameterStore_->Update(parameterAssetId_, desired, editing)) {
         throw std::runtime_error(parameterStore_->GetLastError());
      }
      parameterRevision_ = parameterStore_->Find(parameterAssetId_)->revision;
      parameterValues_ = parameterStore_->Find(parameterAssetId_)->data;
      parameterError_.clear();
      return true;
   } catch (const std::exception& error) {
      parameterError_ = error.what();
      ApplySharedParameters(before);
      return false;
   }
}

} // namespace GameEngine
