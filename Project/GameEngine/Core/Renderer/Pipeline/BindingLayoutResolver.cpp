#include "BindingLayoutResolver.h"

namespace GameEngine {

std::vector<std::string> BindingLayoutResolver::GetExpectedSemanticsForRootSignature(const std::string& rootSignatureName) const {
   (void)rootSignatureName;
   return {};
}

void BindingLayoutResolver::BuildPipelineRootParameterTables(
   std::unordered_map<std::string, PipelineRootParameterTable>& output,
   const std::function<const ShaderReflectionInfo*(const std::string&, ShaderType)>& getReflection) const {
   (void)getReflection;
   output.clear();
}

}
