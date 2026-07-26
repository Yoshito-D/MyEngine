#include "PipelineDefinitionLoader.h"
#include "Utility/Logger.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <filesystem>
#include <Windows.h>

using json = nlohmann::json;

namespace {
std::string WStringToString(const std::wstring& wstr) {
   if (wstr.empty()) {
      return std::string();
   }

   int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], static_cast<int>(wstr.size()), nullptr, 0, nullptr, nullptr);
   std::string value(static_cast<size_t>(sizeNeeded), 0);
   WideCharToMultiByte(CP_UTF8, 0, &wstr[0], static_cast<int>(wstr.size()), &value[0], sizeNeeded, nullptr, nullptr);
   return value;
}
}

namespace GameEngine {

bool PipelineDefinitionLoader::LoadRegistryFile(const std::wstring& registryFilePath, std::vector<std::string>& rootSignaturePaths, std::vector<std::string>& pipelinePaths) const {
   rootSignaturePaths.clear();
   pipelinePaths.clear();

   std::ifstream file(WStringToString(registryFilePath));
   if (!file.is_open()) {
      Logger::Error("[PipelineDefinitionLoader] Failed to open pipeline registry: " + WStringToString(registryFilePath));
      return false;
   }

   try {
      json registryJson;
      file >> registryJson;

      if (!registryJson.contains("rootSignatures") || !registryJson["rootSignatures"].is_array()) {
         Logger::Error("[PipelineDefinitionLoader] Pipeline registry is missing required array: rootSignatures");
         return false;
      }
      if (!registryJson.contains("pipelines") || !registryJson["pipelines"].is_array()) {
         Logger::Error("[PipelineDefinitionLoader] Pipeline registry is missing required array: pipelines");
         return false;
      }

      bool allSucceeded = true;
      const auto loadPathArray = [&](const json& paths, const char* label, std::vector<std::string>& output) {
         size_t index = 0;
         for (const auto& pathJson : paths) {
            const std::string entryLabel = std::string(label) + "[" + std::to_string(index) + "]";
            ++index;

            if (!pathJson.is_string()) {
               Logger::Error("[PipelineDefinitionLoader] Registry path entry is not a string: " + entryLabel);
               allSucceeded = false;
               continue;
            }

            const std::string path = pathJson.get<std::string>();
            if (!std::filesystem::exists(path)) {
               Logger::Error("[PipelineDefinitionLoader] Registry path does not exist: " + path + " (" + entryLabel + ")");
               allSucceeded = false;
               continue;
            }

            // 正常な項目は保持しつつ全体は失敗扱いにし、ログで複数の設定不備を一度に確認できるようにする。
            output.push_back(path);
         }
      };

      loadPathArray(registryJson["rootSignatures"], "rootSignatures", rootSignaturePaths);
      loadPathArray(registryJson["pipelines"], "pipelines", pipelinePaths);

      if (rootSignaturePaths.empty()) {
         Logger::Error("[PipelineDefinitionLoader] Pipeline registry did not provide any valid root signature paths.");
         allSucceeded = false;
      }
      if (pipelinePaths.empty()) {
         Logger::Error("[PipelineDefinitionLoader] Pipeline registry did not provide any valid pipeline paths.");
         allSucceeded = false;
      }

      return allSucceeded;
   } catch (const std::exception& e) {
      Logger::Error("[PipelineDefinitionLoader] Exception loading pipeline registry: " + std::string(e.what()));
      return false;
   } catch (...) {
      Logger::Error("[PipelineDefinitionLoader] Unknown exception loading pipeline registry.");
      return false;
   }
}

}
