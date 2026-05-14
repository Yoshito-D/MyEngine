#include "PipelineDefinitionLoader.h"
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
      return false;
   }

   try {
      json registryJson;
      file >> registryJson;

      if (registryJson.contains("rootSignatures")) {
         for (const auto& rootSigPath : registryJson["rootSignatures"]) {
            rootSignaturePaths.push_back(ResolvePath(rootSigPath.get<std::string>()));
         }
      }

      if (registryJson.contains("pipelines")) {
         for (const auto& pipelinePath : registryJson["pipelines"]) {
            pipelinePaths.push_back(ResolvePath(pipelinePath.get<std::string>()));
         }
      }

      return true;
   } catch (...) {
      return false;
   }
}

std::string PipelineDefinitionLoader::ResolvePath(const std::string& path) const {
   if (std::filesystem::exists(path)) {
      return path;
   }

   std::string resolved = path;

   const std::string legacyRootSig = "resources/pipelines/rootsignatures/";
   if (resolved.rfind(legacyRootSig, 0) == 0) {
      resolved = "resources/pipelines/rootsig/" + resolved.substr(legacyRootSig.size());
      if (std::filesystem::exists(resolved)) {
         return resolved;
      }
   }

   const std::string legacyPipelineFolder = "resources/pipelines/pipelines/";
   if (resolved.rfind(legacyPipelineFolder, 0) == 0) {
      resolved = "resources/pipelines/" + resolved.substr(legacyPipelineFolder.size());
      if (std::filesystem::exists(resolved)) {
         return resolved;
      }
   }

   return path;
}

}
