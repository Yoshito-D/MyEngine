#pragma once

#include <filesystem>
#include <vector>

namespace GameEngine {

/// @brief 規定のディレクトリからパイプライン定義を検出し、識別名を検証する。
class PipelineDefinitionLoader {
public:
   /// @brief root_signaturesとpso以下のJSONを再帰的に検出し、パス順に並べて返す。
   /// @param pipelineDirectory パイプライン定義のルートディレクトリ。
   /// @param rootSignaturePaths 検出したルートシグネチャ定義のパス。
   /// @param pipelinePaths 検出したPSO定義のパス。
   /// @return 両ディレクトリに有効な定義があり、各種類でnameが一意ならtrue。失敗時は両出力を空にする。
   bool DiscoverDefinitions(const std::filesystem::path& pipelineDirectory,
      std::vector<std::filesystem::path>& rootSignaturePaths, std::vector<std::filesystem::path>& pipelinePaths) const;
};

}
