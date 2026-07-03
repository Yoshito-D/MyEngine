#pragma once
#include <string>
#include <vector>

namespace GameEngine {

class PipelineDefinitionLoader {
public:
   // @brief レジストリファイルを読み込み、ルートシグネチャとパイプラインのパスを取得する
   // @param registryFilePath レジストリファイルのパス
   // @param rootSignaturePaths ルートシグネチャのパスを格納するベクター
   bool LoadRegistryFile(const std::wstring& registryFilePath, std::vector<std::string>& rootSignaturePaths, std::vector<std::string>& pipelinePaths) const;

private:
   // @brief パスを解決する（存在しない場合はレガシーパスを試す）
   // @param path 解決するパス
   std::string ResolvePath(const std::string& path) const;
};

}
