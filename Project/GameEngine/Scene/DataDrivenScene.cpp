#include "pch.h"
#include "DataDrivenScene.h"

#include "Utility/Logger.h"
#include <fstream>

namespace GameEngine {

DataDrivenScene::DataDrivenScene(std::filesystem::path sceneFilePath)
   : sceneFilePath_(std::move(sceneFilePath)) {
}

void DataDrivenScene::LoadSceneDataIfNeeded() {
   if (isLoaded_) {
      return;
   }

   std::ifstream file(sceneFilePath_);
   if (!file.is_open()) {
      Logger::Error("DataDrivenScene could not open: " + sceneFilePath_.generic_string());
      return;
   }

   nlohmann::json sceneData;
   try {
      file >> sceneData;
   } catch (const nlohmann::json::exception& exception) {
      Logger::Error("DataDrivenScene contains invalid JSON: " + std::string(exception.what()));
      return;
   }

   isLoaded_ = sceneWorld_.LoadFromJson(sceneData);
   if (!isLoaded_) {
      Logger::Error("DataDrivenScene failed to build: " + sceneFilePath_.generic_string());
   }
}

void DataDrivenScene::OnUpdate(float deltaTime) {
   if (isLoaded_) {
      sceneWorld_.Update(deltaTime);
   }
}

void DataDrivenScene::OnFinalize() {
   sceneWorld_.Clear();
   isLoaded_ = false;
}

} // namespace GameEngine
