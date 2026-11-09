#include "GameEngine/pch.h"
#include "GameEngine/UI/UIScreenAssetSerializer.h"
#include "GameEngine/UI/UIComponents.h"
#include "GameEngine/Object/Object.h"
#include "GameEngine/Scene/SceneWorld.h"
#include <filesystem>
#include <fstream>
#include <unordered_set>

namespace GameEngine {
namespace {
constexpr std::int32_t kScreenAssetVersion = 1;

std::filesystem::path ScreenPath(const std::string& assetId) {
   const std::filesystem::path path(std::u8string(assetId.begin(), assetId.end()));

   if (path.is_absolute() || path.has_root_name() || path.extension() != ".json") {
      throw std::runtime_error("UI asset must be a relative JSON path under game/ui");
   }

   for (const auto& part : path) {
      if (part == "..") {
         throw std::runtime_error("UI asset cannot contain parent traversal");
      }
   }

   const auto normalized = path.lexically_normal().generic_u8string();

   if (!normalized.starts_with(u8"game/ui/")) {
      throw std::runtime_error("UI asset must be stored under game/ui");
   }

   return std::filesystem::path("resources") / path;
}

bool HasComponent(const nlohmann::json& object, const char* type) {
   for (const auto& component : object.value("components", nlohmann::json::array())) {
      if (component.value("typeName", "") == type) {
         return true;
      }
   }

   return false;
}
}

namespace {
// 保存と読込を同じ非破壊検証に通し、読めない画面アセットを書き出さない。
void ValidateScreenAsset(const nlohmann::json& asset) {
   if (!asset.is_object() || asset.at("version") != kScreenAssetVersion || !asset.at("objects").is_array()) {
      throw std::runtime_error("Invalid UI screen asset");
   }

   const std::string rootId = asset.at("rootId").get<std::string>();
   std::unordered_map<std::string, std::string> parents;
   std::unordered_set<std::string> keys;
   bool validRoot = false;

   for (const auto& object : asset["objects"]) {
      const auto id = object.at("id").get<std::string>();

      if (id.empty() || !parents.emplace(id, object.value("parentId", "")).second) {
         throw std::runtime_error("Duplicate or empty UI object ID");
      }

      const auto type = object.at("objectType").get<std::string>();

      if (type != "Generic" && type != "Sprite" && type != "UIText") {
         throw std::runtime_error("Unsupported UI object type");
      }

      for (const auto& component : object.at("components")) {
         const auto name = component.at("typeName").get<std::string>();
         static const std::unordered_set<std::string> allowed{"ObjectNameComponent", "TransformComponent",
            "RenderComponent", "MaterialComponent", "MeshComponent", "UITextComponent", "UIFadeComponent",
            "UITransformTweenComponent", "TypewriterComponent", "UICanvasComponent", "UIRectTransformComponent",
            "UIGroupComponent", "UILayoutGroupComponent", "UIScreenComponent", "UIButtonComponent", "UISliderComponent",
            "UIToggleComponent", "UIAnimationComponent"};

         if (!allowed.contains(name)) {
            throw std::runtime_error("UI assets cannot contain game components: " + name);
         }

         if (name == "UIRectTransformComponent") {
            const auto key = component.value("data", nlohmann::json::object()).value("elementKey", "");

            if (!key.empty() && !keys.insert(key).second) {
               throw std::runtime_error("Duplicate UI element key: " + key);
            }
         }
      }

      if (HasComponent(object, UICanvasComponent::kTypeName)) {
         if (id != rootId) {
            throw std::runtime_error("Nested UI canvases are not supported in screen assets");
         }

         validRoot = HasComponent(object, UIScreenComponent::kTypeName);
      }
   }

   if (!validRoot || !parents.contains(rootId) || !parents.at(rootId).empty()) {
      throw std::runtime_error("Invalid UI root");
   }

   for (const auto& [id, parent] : parents) {
      std::unordered_set<std::string> visited;
      auto current = id;

      while (current != rootId) {
         if (!visited.insert(current).second || !parents.contains(current)) {
            throw std::runtime_error("Invalid UI parent hierarchy");
         }

         current = parents.at(current);
      }
   }
}
}

bool UIScreenAssetSerializer::Validate(const nlohmann::json& asset, std::string& error) {
   try {
      ValidateScreenAsset(asset);
      error.clear();
      return true;
   } catch (const std::exception& failure) {
      error = failure.what();
      return false;
   }
}

nlohmann::json UIScreenAssetSerializer::Load(const std::string& assetId, std::string& error) {
   try {
      std::ifstream input(ScreenPath(assetId));

      if (!input) {
         throw std::runtime_error("UI screen file could not be opened: " + assetId);
      }

      nlohmann::json asset;
      input >> asset;
      return Validate(asset, error) ? asset : nlohmann::json{};
   } catch (const std::exception& failure) {
      error = failure.what();
      return {};
   }
}

nlohmann::json UIScreenAssetSerializer::Capture(
   const SceneWorld& world, const std::string& rootId, std::string& error, const std::vector<std::string>& order) {
   const auto* root = world.FindObjectById(rootId);

   if (!root || !root->GetComponent<UICanvasComponent>() || !root->GetComponent<UIScreenComponent>()) {
      error = "Select a UI Canvas with a UI Screen component";
      return {};
   }

   auto asset = world.GetObjectStore().CaptureSubtree(rootId, error, order);

   if (asset.empty()) {
      return {};
   }

   asset["version"] = kScreenAssetVersion;

   for (auto& object : asset["objects"]) {
      if (object["id"] == rootId) {
         object["parentId"] = "";
      }
   }

   return Validate(asset, error) ? asset : nlohmann::json{};
}

std::string UIScreenAssetSerializer::Instantiate(SceneWorld& world, const std::string& assetId, std::string& error) {
   const auto asset = Load(assetId, error);
   return asset.empty() ? std::string{} : world.GetObjectStore().RestoreSubtree(asset, true, error);
}

bool UIScreenAssetSerializer::Save(const SceneWorld& world, const std::string& rootId, const std::string& assetId,
   std::string& error, const std::vector<std::string>& order) {
   try {
      auto asset = Capture(world, rootId, error, order);

      if (asset.empty()) {
         return false;
      }

      const auto path = ScreenPath(assetId);
      std::filesystem::create_directories(path.parent_path());
      auto temporary = path;
      temporary += ".tmp";
      {
         std::ofstream output(temporary, std::ios::binary);

         for (const char value : asset.dump(3)) {
            if (value == '\n') {
               output.put('\r');
            }

            output.put(value);
         }

         output << "\r\n";
         output.flush();

         if (!output) {
            throw std::runtime_error("Failed to write UI screen");
         }
      }

      if (!MoveFileExW(temporary.c_str(), path.c_str(), MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)) {
         std::filesystem::remove(temporary);
         throw std::runtime_error("Failed to replace UI screen file");
      }

      error.clear();
      return true;
   } catch (const std::exception& failure) {
      error = failure.what();
      return false;
   }
}
}
