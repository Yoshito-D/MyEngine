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
      // 初期化と更新の両方から呼ばれても、同じオブジェクトを二重生成しない。
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

   // 構築に失敗した場合は未ロードのままにし、呼び出し側が再試行できる状態を残す。
   isLoaded_ = sceneWorld_.LoadFromJson(sceneData);
   if (!isLoaded_) {
      Logger::Error("DataDrivenScene failed to build: " + sceneFilePath_.generic_string());
      return;
   }

#ifdef USE_IMGUI
   if (auto* editorContext = GetEditorSceneContext()) {
      // データ駆動シーンはBaseSceneの自動読込を通らないため、表示順だけを重複生成なしで渡す。
      editorContext->ApplyHierarchyOrder(
         sceneData.value("hierarchyOrder", nlohmann::json::array()));
   }
#endif
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
