#pragma once
#include <string>
#include <vector>

namespace GameEngine {

class PipelineDefinitionLoader {
public:
   /// @brief レジストリファイルを読み込み、ルートシグネチャとパイプラインのパスを取得する
   /// @param registryFilePath レジストリファイルのパス
   /// @param rootSignaturePaths ルートシグネチャのパスを格納するベクター
   /// @param pipelinePaths パイプライン定義のパスを格納するベクター
   /// @return レジストリと参照先ファイルをすべて読み取れた場合はtrue
   bool LoadRegistryFile(const std::wstring& registryFilePath, std::vector<std::string>& rootSignaturePaths, std::vector<std::string>& pipelinePaths) const;
};

}
