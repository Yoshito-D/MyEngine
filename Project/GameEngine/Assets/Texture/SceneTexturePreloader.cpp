#include "GameEngine/pch.h"
#include "GameEngine/Assets/Texture/SceneTexturePreloader.h"
#include "GameEngine/Assets/Texture/TextureManager.h"
#include "GameEngine/Assets/ResourceAssetPath.h"
#include <algorithm>
#include <fstream>
#include <nlohmann/json.hpp>
#include <set>
#include <stdexcept>

namespace GameEngine {
namespace {
void CollectSettings(const nlohmann::json& value, std::set<std::string>& textures, std::set<std::string>& visited);

void AddTexture(const std::string& id, std::set<std::string>& textures) {
   if (!id.empty()) {
      textures.insert(id);
   }
}

void CollectParticleFile(
   const std::string& reference, std::set<std::string>& textures, std::set<std::string>& visited) {
   if (reference.empty()) return;
   std::string id = reference;
   std::replace(id.begin(), id.end(), '\\', '/');
   if (id.starts_with("Resources/") || id.starts_with("resources/")) {
      id.erase(0, 10);
   }
   // サブエミッターの循環と複数スロットからの共有参照を1回の読み込みへまとめる。
   if (!visited.insert(id).second) return;
   const auto path = ResolveResourceAssetPath(id);
   if (path.empty() || path.extension() != ".json") {
      throw std::runtime_error("Particle dependency is missing or outside Resources: " + reference);
   }
   std::ifstream input(path);
   nlohmann::json settings;
   input >> settings;
   if (!settings.is_object()) {
      throw std::runtime_error("Invalid particle dependency: " + reference);
   }
   CollectSettings(settings, textures, visited);
}

void CollectSettings(const nlohmann::json& value, std::set<std::string>& textures, std::set<std::string>& visited) {
   if (value.is_array()) {
      for (const auto& element : value) {
         CollectSettings(element, textures, visited);
      }
      return;
   }
   if (!value.is_object()) return;
   for (auto it = value.begin(); it != value.end(); ++it) {
      const auto& key = it.key();
      const auto& field = it.value();
      if ((key == "textureName" || key == "environmentTextureName" || key == "maskTexture") && field.is_string()) {
         AddTexture(field.get<std::string>(), textures);
      } else if (key == "textureNames" && field.is_array()) {
         for (const auto& id : field) {
            if (id.is_string()) {
               AddTexture(id.get<std::string>(), textures);
            }
         }
      } else if (key == "jsonPath" && field.is_string()) {
         CollectParticleFile(field.get<std::string>(), textures, visited);
      } else if (key == "subEmitters" && field.is_object()) {
         if (field.value("enabled", false)) {
            for (const auto* name : {"spawnOnDeathPath", "spawnOnUpdatePath", "spawnOnCollisionPath"}) {
               CollectParticleFile(field.value(name, ""), textures, visited);
            }
         }
      } else {
         CollectSettings(field, textures, visited);
      }
   }
}
}

bool PreloadSceneTextures(TextureManager& textures, const nlohmann::json& scene) {
   try {
      // 収集状態は読み込み1回だけに所有させ、復元や再読込で古い設定を再利用しない。
      std::set<std::string> ids;
      std::set<std::string> visited;
      CollectSettings(scene.value("renderSettings", nlohmann::json::object()), ids, visited);
      for (const auto& object : scene.at("objects")) {
         const auto type = object.value("objectType", "");
         if (type == "Sprite") {
            AddTexture(object.value("assetId", ""), ids);
         } else if (type == "ParticleSystem" && !(object.contains("data") && object.at("data").is_object())) {
            CollectParticleFile(object.value("assetId", ""), ids, visited);
         }
         // 完全なParticleスナップショットがあればassetIdのテンプレートは読まない。
         CollectSettings(object, ids, visited);
      }
      const std::vector<std::string> orderedIds(ids.begin(), ids.end());
      return textures.Preload(orderedIds);
   } catch (const std::exception& error) {
      Logger::Error("Scene texture preparation failed: " + std::string(error.what()));
      return false;
   }
}
}
