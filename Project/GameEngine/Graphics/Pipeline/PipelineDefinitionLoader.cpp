#include "GameEngine/Graphics/Pipeline/PipelineDefinitionLoader.h"
#include "GameEngine/Utility/Logger.h"
#include <nlohmann/json.hpp>
#include <algorithm>
#include <fstream>
#include <unordered_map>

namespace GameEngine {

bool PipelineDefinitionLoader::DiscoverDefinitions(const std::filesystem::path& pipelineDirectory,
   std::vector<std::filesystem::path>& rootSignaturePaths, std::vector<std::filesystem::path>& pipelinePaths) const {
   rootSignaturePaths.clear();
   pipelinePaths.clear();

   const auto discover = [](const std::filesystem::path& directory, std::vector<std::filesystem::path>& paths) {
      for (const auto& entry : std::filesystem::recursive_directory_iterator(directory)) {
         if (entry.is_regular_file() && entry.path().extension() == L".json") {
            paths.push_back(entry.path());
         }
      }

      if (paths.empty()) {
         Logger::Error(L"[PipelineDefinitionLoader] No JSON definitions found in: " + directory.wstring());
         return false;
      }

      // ファイルシステムの列挙順に依存せず、同名定義を後勝ちで上書きしない。
      // GPUオブジェクトの生成前に全件を検証し、不正な定義集合をまとめて拒否する。
      std::sort(paths.begin(), paths.end());
      std::unordered_map<std::string, std::filesystem::path> names;

      for (const auto& path : paths) {
         try {
            std::ifstream file(path);

            if (!file.is_open()) {
               Logger::Error(L"[PipelineDefinitionLoader] Cannot read definition: " + path.wstring());
               return false;
            }

            nlohmann::json definition;
            file >> definition;
            const std::string name = definition.at("name").get<std::string>();

            if (name.empty()) {
               Logger::Error(L"[PipelineDefinitionLoader] Definition name is empty: " + path.wstring());
               return false;
            }

            const auto [previous, inserted] = names.emplace(name, path);

            if (!inserted) {
               Logger::Error(L"[PipelineDefinitionLoader] Duplicate definition name: " + path.wstring() +
                             L" (already defined in " + previous->second.wstring() + L")");
               return false;
            }
         } catch (const std::exception& error) {
            Logger::Error(L"[PipelineDefinitionLoader] Invalid definition: " + path.wstring());
            Logger::Error(error.what());
            return false;
         }
      }

      return true;
   };

   try {
      std::vector<std::filesystem::path> roots;
      std::vector<std::filesystem::path> pipelines;

      if (!discover(pipelineDirectory / "root_signatures", roots) || !discover(pipelineDirectory / "pso", pipelines)) {
         return false;
      }

      rootSignaturePaths.swap(roots);
      pipelinePaths.swap(pipelines);
      Logger::Info("[PipelineDefinitionLoader] Discovered " + std::to_string(rootSignaturePaths.size()) +
                   " root signatures and " + std::to_string(pipelinePaths.size()) + " pipelines.");
      return true;
   } catch (const std::exception& error) {
      Logger::Error(L"[PipelineDefinitionLoader] Cannot scan pipeline directory: " + pipelineDirectory.wstring());
      Logger::Error(error.what());
      return false;
   }
}

}
