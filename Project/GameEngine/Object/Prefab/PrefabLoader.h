#pragma once

#include "Object.h"
#include <filesystem>
#include <fstream>
#include <memory>
#include <nlohmann/json.hpp>

namespace GameEngine {
class PrefabLoader {
public:
   static std::unique_ptr<Object> LoadObjectFromFile(const std::filesystem::path& prefabPath) {
      nlohmann::json prefabJson;
      if (!LoadJson(prefabPath, prefabJson)) {
         return nullptr;
      }

      auto object = std::make_unique<Object>();
      ApplyPrefabJson(*object, prefabJson);
      return object;
   }

   static bool ApplyToObject(Object& object, const std::filesystem::path& prefabPath) {
      nlohmann::json prefabJson;
      if (!LoadJson(prefabPath, prefabJson)) {
         return false;
      }

      return ApplyPrefabJson(object, prefabJson);
   }

private:
   static bool LoadJson(const std::filesystem::path& path, nlohmann::json& outJson) {
      std::ifstream stream(path);
      if (!stream.is_open()) {
         return false;
      }

      try {
         stream >> outJson;
      } catch (...) {
         return false;
      }

      return outJson.is_object();
   }

   static bool ApplyPrefabJson(Object& object, const nlohmann::json& prefabJson) {
      if (!prefabJson.contains("components") || !prefabJson.at("components").is_array()) {
         return false;
      }

      return object.DeserializeComponents(prefabJson.at("components"));
   }
};
}
