#include "GameEngine/pch.h"
#include "GameEngine/Assets/ParameterAssetStore.h"
#include "GameEngine/Assets/ResourceAssetPath.h"
#include "GameEngine/Utility/JsonFile.h"
#include "GameEngine/Utility/Logger.h"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>

namespace GameEngine {

ParameterAssetStore::ParameterAssetStore(std::filesystem::path resourcesRoot)
   : resourcesRoot_(std::move(resourcesRoot)) {
}

bool ParameterAssetStore::Fail(std::string message) {
   lastError_ = std::move(message);
   Logger::Warning("Parameter asset: " + lastError_);
   return false;
}

bool ParameterAssetStore::Validate(const nlohmann::json& expected, const nlohmann::json& data) {
   if (expected.is_number()) {
      if (!data.is_number()) {
         return false;
      }
      const double value = data.get<double>();
      return std::isfinite(value) && std::abs(value) <= std::numeric_limits<float>::max() &&
             (!expected.is_number_integer() ||
                (data.is_number_integer() && (!expected.is_number_unsigned() || value >= 0.0)));
   }
   if (expected.type() != data.type()) {
      return false;
   }
   if (data.is_object()) {
      if (expected.size() != data.size()) {
         return false;
      }
      for (auto it = expected.begin(); it != expected.end(); ++it) {
         if (!data.contains(it.key()) || !Validate(it.value(), data.at(it.key()))) {
            return false;
         }
      }
   } else if (data.is_array()) {
      const bool fixedVector = !expected.empty() && expected.front().is_number();
      if (fixedVector && expected.size() != data.size()) {
         return false;
      }
      for (const auto& item : data) {
         // スロット配列は可変長で、各型の適用処理が内容を検証する。
         const auto& schema = expected.empty() || expected.front().is_object() ? item : expected.front();
         if (!Validate(schema, item)) {
            return false;
         }
      }
   }
   return true;
}

std::filesystem::path ParameterAssetStore::ResolveWritePath(const std::string& id) const {
   const auto relative = ResourceAssetIdPath(id);
   if (relative.empty() || !id.starts_with("game/parameters/") || relative.extension() != ".json") {
      return {};
   }
   std::error_code error;
   const auto root = std::filesystem::weakly_canonical(resourcesRoot_, error);
   if (error) {
      return {};
   }
   const auto candidate = std::filesystem::weakly_canonical(root / relative, error);
   if (error) {
      return {};
   }
   auto part = candidate.begin();
   for (const auto& rootPart : root) {
      if (part == candidate.end() || *part++ != rootPart) {
         return {};
      }
   }
   return part == candidate.end() ? std::filesystem::path{} : candidate;
}

const ParameterAsset* ParameterAssetStore::Find(const std::string& id) const {
   const auto it = assets_.find(id);
   return it == assets_.end() ? nullptr : &it->second;
}

const ParameterAsset* ParameterAssetStore::Load(
   const std::string& id, const std::string& domain, const std::string& type) {
   lastError_.clear();
   if (!Find(id)) {
      const auto path = ResolveWritePath(id);
      try {
         std::ifstream input(path);
         if (!input) {
            throw std::runtime_error("file is missing or cannot be opened");
         }
         nlohmann::json file;
         input >> file;
         if (!file.is_object() || file.value("version", 0) != 1 || !file.contains("domain") ||
             !file.at("domain").is_string() || !file.contains("componentType") ||
             !file.at("componentType").is_string() || !file.contains("data") || !file.at("data").is_object() ||
             file.at("data").empty() || !Validate(file.at("data"), file.at("data"))) {
            throw std::runtime_error("invalid parameter document");
         }
         ParameterAsset asset;
         asset.domain = file.at("domain").get<std::string>();
         asset.componentType = file.at("componentType").get<std::string>();
         asset.data = file.at("data");
         asset.savedData = asset.data;
         asset.saved = true;
         assets_.emplace(id, std::move(asset));
      } catch (const std::exception& error) {
         Fail("Could not load " + id + ": " + error.what());
         return nullptr;
      }
   }
   const auto* asset = Find(id);
   if (asset->domain != domain || asset->componentType != type) {
      Fail("Component type mismatch: " + id);
      return nullptr;
   }
   return asset;
}

bool ParameterAssetStore::Update(const std::string& id, const nlohmann::json& data, bool editing) {
   auto it = assets_.find(id);
   if (it == assets_.end() || !Validate(it->second.data, data)) {
      return Fail("Invalid edited settings: " + id);
   }
   auto& asset = it->second;
   if (asset.data != data) {
      if (!asset.editing) {
         asset.undo.push_back(asset.data);
      }
      asset.redo.clear();
      asset.data = data;
      ++asset.revision;
      asset.editing = editing;
   } else if (!editing) {
      // 押下だけで値が変わらないフレームでは履歴グループを開始しない。
      asset.editing = false;
   }
   lastError_.clear();
   return true;
}

void ParameterAssetStore::EndEdit(const std::string& id) {
   const auto it = assets_.find(id);
   if (it != assets_.end()) {
      it->second.editing = false;
   }
}

bool ParameterAssetStore::Undo(const std::string& id) {
   auto it = assets_.find(id);
   if (it == assets_.end() || it->second.undo.empty()) {
      return false;
   }
   auto& asset = it->second;
   asset.redo.push_back(asset.data);
   asset.data = std::move(asset.undo.back());
   asset.undo.pop_back();
   asset.editing = false;
   ++asset.revision;
   return true;
}

bool ParameterAssetStore::Redo(const std::string& id) {
   auto it = assets_.find(id);
   if (it == assets_.end() || it->second.redo.empty()) {
      return false;
   }
   auto& asset = it->second;
   asset.undo.push_back(asset.data);
   asset.data = std::move(asset.redo.back());
   asset.redo.pop_back();
   asset.editing = false;
   ++asset.revision;
   return true;
}

std::string ParameterAssetStore::Create(
   const std::string& domain, const std::string& type, const nlohmann::json& data) {
   if ((domain != "Object" && domain != "Camera") || !data.is_object() || data.empty() || !Validate(data, data)) {
      Fail("This component has no valid shared settings");
      return {};
   }
   for (std::uint32_t index = 1; index != 100000; ++index) {
      const std::string id = "game/parameters/custom/" + domain + "_" + type + "_" + std::to_string(index) + ".json";
      const auto path = ResolveWritePath(id);
      if (path.empty()) {
         Fail("Invalid component type or resource path");
         return {};
      }
      std::error_code error;
      const bool exists = std::filesystem::exists(path, error);
      if (error) {
         Fail("Could not inspect parameter destination: " + error.message());
         return {};
      }
      if (!Find(id) && !exists) {
         ParameterAsset asset;
         asset.domain = domain;
         asset.componentType = type;
         asset.data = data;
         assets_.emplace(id, std::move(asset));
         // シーンへ割り当てるIDは、実在するJSONの保存に成功してから公開する。
         // 保存できない場合は候補にも残さず、存在しないファイルへの参照を防ぐ。
         if (!Save(id)) {
            assets_.erase(id);
            return {};
         }
         return id;
      }
   }
   Fail("Could not allocate a parameter asset ID");
   return {};
}

bool ParameterAssetStore::Save(const std::string& id) {
   auto it = assets_.find(id);
   const auto path = ResolveWritePath(id);
   if (it == assets_.end() || path.empty()) {
      return Fail("Invalid save destination: " + id);
   }
   std::error_code error;
   std::filesystem::create_directories(path.parent_path(), error);
   auto& asset = it->second;
   const nlohmann::json file{
      {"version", 1}, {"domain", asset.domain}, {"componentType", asset.componentType}, {"data", asset.data}};
   if (error || !SaveJsonFileAtomically(path, file)) {
      return Fail("Could not save " + id);
   }
   asset.savedData = asset.data;
   asset.saved = true;
   asset.editing = false;
   lastError_.clear();
   return true;
}

bool ParameterAssetStore::SaveAll() {
   std::string failures;
   for (const auto& id : GetDirtyIds()) {
      if (!Save(id)) {
         if (!failures.empty()) {
            failures += "\n";
         }
         failures += lastError_;
      }
   }
   lastError_ = std::move(failures);
   return lastError_.empty();
}

std::vector<std::string> ParameterAssetStore::List(const std::string& domain, const std::string& type) {
   std::error_code error;
   const auto directory = resourcesRoot_ / "game/parameters";
   for (std::filesystem::recursive_directory_iterator it(directory, error), end; it != end && !error;
      it.increment(error)) {
      if (it->is_regular_file(error) && it->path().extension() == ".json") {
         const auto id = ResourcePathToUtf8(it->path().lexically_relative(resourcesRoot_));
         if (ResolveWritePath(id).empty() || Find(id)) {
            continue;
         }
         // 他型のファイルは候補から除く。型不一致をエラーとして表示しない。
         try {
            std::ifstream input(it->path());
            nlohmann::json file;
            input >> file;
            if (file.value("domain", "") == domain && file.value("componentType", "") == type) {
               Load(id, domain, type);
            }
         } catch (const std::exception&) {
            continue;
         }
      }
   }
   std::vector<std::string> ids;
   for (const auto& [id, asset] : assets_) {
      if (asset.domain == domain && asset.componentType == type) {
         ids.push_back(id);
      }
   }
   return ids;
}

std::vector<std::string> ParameterAssetStore::GetDirtyIds() const {
   std::vector<std::string> ids;
   for (const auto& [id, asset] : assets_) {
      if (asset.IsDirty()) {
         ids.push_back(id);
      }
   }
   return ids;
}

std::string ParameterAssetStore::ResolveObjectAssetId(const nlohmann::json& object) {
   const auto type = object.value("objectType", "");
   const std::string componentType = type == "Model" ? "MeshComponent" : "MaterialComponent";
   if ((type == "Model" || type == "Sprite") && object.contains("components")) {
      for (const auto& entry : object.at("components")) {
         if (entry.value("typeName", "") != componentType || !entry.contains("parameterAssetId")) {
            continue;
         }
         const auto* asset = Load(entry.at("parameterAssetId").get<std::string>(), "Object", componentType);
         if (!asset) {
            throw std::runtime_error(lastError_);
         }
         if (type == "Model") {
            return asset->data.value("sourceType", "") == "ModelFile" ? asset->data.value("assetId", "") : "";
         }
         const auto slots = asset->data.value("materialSlots", nlohmann::json::array());
         return slots.empty() ? "" : slots.front().value("textureName", "");
      }
   }
   return object.value("assetId", "");
}

nlohmann::json ParameterAssetStore::ResolveScene(const nlohmann::json& scene) {
   auto result = scene;
   const auto resolve = [this](nlohmann::json& entries, const char* domain, const char* typeKey) {
      for (auto& entry : entries) {
         if (!entry.contains("parameterAssetId")) {
            continue;
         }
         const auto id = entry.at("parameterAssetId").get<std::string>();
         const auto* asset = Load(id, domain, entry.at(typeKey).get<std::string>());
         if (!asset) {
            throw std::runtime_error(lastError_);
         }
         auto data = asset->data;
         const auto local = entry.value("data", nlohmann::json::object());
         for (auto it = local.begin(); it != local.end(); ++it) {
            if (data.contains(it.key())) {
               throw std::runtime_error("Shared parameter duplicated in scene: " + id + "/" + it.key());
            }
            data[it.key()] = it.value();
         }
         entry["data"] = std::move(data);
      }
   };
   for (auto& object : result.at("objects")) {
      if (object.contains("components")) {
         resolve(object["components"], "Object", "typeName");
         object["assetId"] = ResolveObjectAssetId(object);
      }
   }
   for (auto& camera : result.at("cameras").at("virtualCameras")) {
      resolve(camera["components"], "Camera", "componentName");
   }
   return result;
}

} // namespace GameEngine
