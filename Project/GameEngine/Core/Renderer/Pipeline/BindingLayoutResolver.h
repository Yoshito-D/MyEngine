#pragma once
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include "ShaderManager.h"

namespace GameEngine {

class BindingLayoutResolver {
public:
   std::vector<std::string> GetExpectedSemanticsForRootSignature(const std::string& rootSignatureName) const;

   void BuildPipelineRootParameterTables(
      std::unordered_map<std::string, PipelineRootParameterTable>& output,
      const std::function<const ShaderReflectionInfo*(const std::string&, ShaderType)>& getReflection) const;
};

}
