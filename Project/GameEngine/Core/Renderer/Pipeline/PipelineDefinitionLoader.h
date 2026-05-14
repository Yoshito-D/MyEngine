#pragma once
#include <string>
#include <vector>

namespace GameEngine {

class PipelineDefinitionLoader {
public:
   bool LoadRegistryFile(const std::wstring& registryFilePath, std::vector<std::string>& rootSignaturePaths, std::vector<std::string>& pipelinePaths) const;

private:
   std::string ResolvePath(const std::string& path) const;
};

}
