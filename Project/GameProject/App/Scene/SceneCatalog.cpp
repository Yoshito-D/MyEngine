#include "SceneCatalog.h"

#include "Utility/Logger.h"
#include <fstream>
#include <nlohmann/json.hpp>

bool SceneCatalog::Load(const std::filesystem::path& catalogPath) {
   scenes_.clear();
   initialSceneName_.clear();

   std::ifstream file(catalogPath);
   if (!file.is_open()) {
      Logger::Error("Scene catalog could not be opened: " + catalogPath.generic_string());
      return false;
   }

   nlohmann::json catalogData;
   try {
      file >> catalogData;
   } catch (const nlohmann::json::exception& exception) {
      Logger::Error("Scene catalog contains invalid JSON: " + std::string(exception.what()));
      return false;
   }

   if (!catalogData.is_object() || !catalogData.contains("scenes") || !catalogData.at("scenes").is_object()) {
      Logger::Error("Scene catalog must contain a scenes object");
      return false;
   }

   initialSceneName_ = catalogData.value("initialScene", "");
   for (const auto& [sceneName, scenePath] : catalogData.at("scenes").items()) {
      if (!sceneName.empty() && scenePath.is_string()) {
         scenes_[sceneName] = std::filesystem::path("resources") / scenePath.get<std::string>();
      }
   }

   if (initialSceneName_.empty() || !Contains(initialSceneName_)) {
      Logger::Error("Scene catalog initialScene is missing or not registered");
      return false;
   }
   return true;
}

std::filesystem::path SceneCatalog::Resolve(const std::string& sceneName) const {
   const auto it = scenes_.find(sceneName);
   return it != scenes_.end() ? it->second : std::filesystem::path();
}

bool SceneCatalog::Contains(const std::string& sceneName) const {
   return scenes_.contains(sceneName);
}
