#include "BindingLayoutResolver.h"

namespace GameEngine {

std::vector<std::string> BindingLayoutResolver::GetExpectedSemanticsForRootSignature(const std::string& rootSignatureName) const {
   // リフレクション主導へ移行済みのため、固定ルートシグネチャー名による意味推測は行わない。
   (void)rootSignatureName;
   return {};
}

void BindingLayoutResolver::BuildPipelineRootParameterTables(
   std::unordered_map<std::string, PipelineRootParameterTable>& output,
   const std::function<const ShaderReflectionInfo*(const std::string&, ShaderType)>& getReflection) const {
   // 現在は実行時のBindingLayout解決を正とし、旧キャッシュテーブルを残さない。
   (void)getReflection;
   output.clear();
}

}
