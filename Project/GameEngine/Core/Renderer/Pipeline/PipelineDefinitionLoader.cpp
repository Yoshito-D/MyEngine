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

   // Windows の UTF-16 パスを JSON／ログで共通利用する UTF-8 へ変換する。
   // 先に必要バイト数を問い合わせ、マルチバイト文字を途中で切らない正確な領域を確保する。
   int sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, &wstr[0], static_cast<int>(wstr.size()), nullptr, 0, nullptr, nullptr);
   std::string value(static_cast<size_t>(sizeNeeded), 0);
   WideCharToMultiByte(CP_UTF8, 0, &wstr[0], static_cast<int>(wstr.size()), &value[0], sizeNeeded, nullptr, nullptr);
   return value;
}
}

namespace GameEngine {

bool PipelineDefinitionLoader::LoadRegistryFile(const std::wstring& registryFilePath, std::vector<std::string>& rootSignaturePaths, std::vector<std::string>& pipelinePaths) const {
   // 呼び出し元の古い結果を混ぜないよう、読み込みの成否にかかわらず今回検証できた項目だけを返す。
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

      // 配列全体を走査して不備をまとめて報告する。エラー後も有効な項目は output に残し、
      // 設定修正時に利用可能な定義と壊れている定義をログから一度に判別できるようにする。
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
         // 配列自体が存在しても有効な定義が 0 件なら描画基盤を構築できないため、全体を失敗扱いにする。
         Logger::Error("[PipelineDefinitionLoader] Pipeline registry did not provide any valid root signature paths.");
         allSucceeded = false;
      }
      if (pipelinePaths.empty()) {
         Logger::Error("[PipelineDefinitionLoader] Pipeline registry did not provide any valid pipeline paths.");
         allSucceeded = false;
      }

      return allSucceeded;
   } catch (const std::exception& e) {
      // JSON 構文、型変換、ファイル I/O の例外を API 境界で bool と診断ログへ変換し、起動処理へ伝播させない。
      Logger::Error("[PipelineDefinitionLoader] Exception loading pipeline registry: " + std::string(e.what()));
      return false;
   } catch (...) {
      Logger::Error("[PipelineDefinitionLoader] Unknown exception loading pipeline registry.");
      return false;
   }
}

}
