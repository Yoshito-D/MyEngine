#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include "ShaderManager.h"

namespace GameEngine {

class BindingLayoutResolver {
public:
   // @brief ルートシグネチャ名に対応する期待されるセマンティクスのリストを取得する
   // @param rootSignatureName ルートシグネチャ名
   // @return 期待されるセマンティクスのリスト
   std::vector<std::string> GetExpectedSemanticsForRootSignature(const std::string& rootSignatureName) const;

   // @brief ルートシグネチャ名に対応する期待されるセマンティクスのリストを取得する（ShaderType指定版）
   // @param output 出力先のマップ
   // @param getReflection ルートシグネチャ名とShaderTypeからShaderReflectionInfoを取得する関数
   void BuildPipelineRootParameterTables(
      std::unordered_map<std::string, PipelineRootParameterTable>& output,
      const std::function<const ShaderReflectionInfo*(const std::string&, ShaderType)>& getReflection) const;
};

}
